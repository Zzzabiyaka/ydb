#pragma once
#include "defs.h"
#include "keyvalue.h"

#include "keyvalue_collector.h"
#include "keyvalue_scheme_flat.h"
#include "keyvalue_simple_db.h"
#include "keyvalue_simple_db_flat.h"
#include "keyvalue_state.h"
#include <ydb/core/tablet_flat/tablet_flat_executed.h>
#include <ydb/core/tablet_flat/flat_database.h>
#include <ydb/core/engine/minikql/flat_local_tx_factory.h>
#include <ydb/core/tablet/tablet_counters_aggregator.h>
#include <ydb/core/tablet/tablet_counters_protobuf.h>
#include <ydb/core/blobstorage/base/blobstorage_events.h>

#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/log.h>
#include <library/cpp/json/json_writer.h>
#include <ydb/core/base/appdata.h>
#include <ydb/core/base/blobstorage.h>
#include <ydb/core/base/tablet_resolver.h>
#include <ydb/public/lib/base/msgbus.h>
#include <ydb/core/protos/blobstorage_controller.pb.h>
#include <ydb/core/protos/services.pb.h>
#include <ydb/core/protos/counters_keyvalue.pb.h>
#include <util/string/escape.h>

// Uncomment the following macro to enable consistency check before every transactions in TTxRequest
//#define KIKIMR_KEYVALUE_CONSISTENCY_CHECKS

namespace NKikimr {
namespace NKeyValue {

constexpr ui64 CollectorErrorInitialBackoffMs = 10;
constexpr ui64 CollectorErrorMaxBackoffMs = 5000;
constexpr ui64 CollectorMaxErrors = 20;
constexpr ui64 PeriodicRefreshMs = 15000;

class TKeyValueFlat : public TActor<TKeyValueFlat>, public NTabletFlatExecutor::TTabletExecutedFlat {
protected:
    struct TTxInit : public NTabletFlatExecutor::ITransaction {
        TActorId KeyValueActorId;
        TKeyValueFlat &Self;

        TTxInit(TActorId keyValueActorId, TKeyValueFlat &keyValueFlat)
            : KeyValueActorId(keyValueActorId)
            , Self(keyValueFlat)
        {}

        TTxType GetTxType() const override { return TXTYPE_INIT; }

        bool Execute(NTabletFlatExecutor::TTransactionContext &txc, const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxInit flat Execute");
            TSimpleDbFlat db(txc.DB);
            if (txc.DB.GetScheme().GetTableInfo(TABLE_ID) == nullptr) {
                LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxInit flat BuildScheme");
                // Init the scheme
                auto &alter = txc.DB.Alter();
                alter.AddTable("kvtable", TABLE_ID);
                alter.AddColumn(TABLE_ID, "key", KEY_TAG, NScheme::TSmallBoundedString::TypeId, false);
                alter.AddColumnToKey(TABLE_ID, KEY_TAG);
                alter.AddColumn(TABLE_ID, "value", VALUE_TAG, NScheme::TString::TypeId, false);
                // Init log batching settings
                alter.SetExecutorAllowLogBatching(true);
                alter.SetExecutorLogFlushPeriod(TDuration::MicroSeconds(500));
                Self.State.Clear();
            } else {
                LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxInit flat ReadDb Tree");
                if (!LoadStateFromDB(Self.State, txc.DB)) {
                    return false;
                }
                if (Self.State.GetIsDamaged()) {
                    Self.Become(&TKeyValueFlat::StateBroken);
                    ctx.Send(Self.Tablet(), new TEvents::TEvPoisonPill);
                    return true;
                }
                Self.State.UpdateResourceMetrics(ctx);
            }
            Self.State.InitExecute(Self.TabletID(), KeyValueActorId, txc.Generation, db, ctx, Self.Info());
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet
                    << " TTxInit flat Execute returns true");
            return true;
        }

        static bool LoadStateFromDB(TKeyValueState& state, NTable::TDatabase& db) {
            state.Clear();
            // Just walk through the DB and read all the keys and values
            const std::array<ui32, 2> tags {{ KEY_TAG, VALUE_TAG }};
            auto mode = NTable::ELookup::GreaterOrEqualThan;
            auto iter = db.Iterate(TABLE_ID, {}, tags, mode);

            if (!db.Precharge(TABLE_ID, {}, {}, tags, 0, -1, -1))
                return false;

            while (iter->Next(NTable::ENext::Data) == NTable::EReady::Data) {
                const auto &row = iter->Row();

                TString key(row.Get(0).AsBuf());
                TString value(row.Get(1).AsBuf());

                state.Load(key, value);
                if (state.GetIsDamaged()) {
                    return true;
                }
            }

            return iter->Last() != NTable::EReady::Page;
        }

        void Complete(const TActorContext &ctx) override {
            Self.InitSchemeComplete(ctx);
            Self.CreatedHook(ctx);
        }
    };

    struct TTxRequest : public NTabletFlatExecutor::ITransaction {
        THolder<TIntermediate> Intermediate;
        TKeyValueFlat *Self;

        TTxRequest(THolder<TIntermediate> intermediate, TKeyValueFlat *keyValueFlat)
            : Intermediate(std::move(intermediate))
            , Self(keyValueFlat)
        {
            Intermediate->Response.SetStatus(NMsgBusProxy::MSTATUS_UNKNOWN);
        }

        TTxType GetTxType() const override { return TXTYPE_REQUEST; }

        bool Execute(NTabletFlatExecutor::TTransactionContext &txc, const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxRequest Execute");
            if (!CheckConsistency(txc)) {
                return false;
            }
            if (Self->State.GetIsDamaged()) {
                return true;
            }
            if (Intermediate->SetExecutorFastLogPolicy) {
                txc.DB.Alter().SetExecutorFastLogPolicy(Intermediate->SetExecutorFastLogPolicy->IsAllowed);
            }
            TSimpleDbFlat db(txc.DB);
            Self->State.RequestExecute(Intermediate, db, ctx, Self->Info());
            return true;
        }

        void Complete(const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << Self->TabletID() << " TTxRequest Complete");
            Self->State.RequestComplete(Intermediate, ctx, Self->Info());
        }

        bool CheckConsistency(NTabletFlatExecutor::TTransactionContext &txc) {
#ifdef KIKIMR_KEYVALUE_CONSISTENCY_CHECKS
            TKeyValueState state;
            if (!TTxInit::LoadStateFromDB(state, txc.DB)) {
                return false;
            }
            Y_VERIFY(!state.IsDamaged());
            state.VerifyEqualIndex(Self->State);
            txc.DB.NoMoreReadsForTx();
            return true;
#else
            Y_UNUSED(txc);
            return true;
#endif
        }
    };

    struct TTxMonitoring : public NTabletFlatExecutor::ITransaction {
        const THolder<NMon::TEvRemoteHttpInfo> Event;
        const TActorId RespondTo;
        TKeyValueFlat *Self;

        TTxMonitoring(THolder<NMon::TEvRemoteHttpInfo> event, const TActorId &respondTo, TKeyValueFlat *keyValue)
            : Event(std::move(event))
            , RespondTo(respondTo)
            , Self(keyValue)
        {}

        bool Execute(NTabletFlatExecutor::TTransactionContext &txc, const TActorContext &ctx) override {
            Y_UNUSED(txc);
            TStringStream str;
            THolder<IEventBase> response;
            TCgiParameters params(Event->Cgi());
            if (params.Has("section")) {
                const TString section = params.Get("section");
                NJson::TJsonValue json;
                if (section == "channelstat") {
                    Self->State.MonChannelStat(json);
                } else {
                    json["Error"] = "invalid json parameter value";
                }
                NJson::WriteJson(&str, &json);
                response = MakeHolder<NMon::TEvRemoteJsonInfoRes>(str.Str());
            } else {
                Self->State.RenderHTMLPage(str);
                response = MakeHolder<NMon::TEvRemoteHttpInfoRes>(str.Str());
            }
            ctx.Send(RespondTo, response.Release());
            return true;
        }

        void Complete(const TActorContext &ctx) override {
            Y_UNUSED(ctx);
        }
    };

    struct TTxStoreCollect : public NTabletFlatExecutor::ITransaction {
        TKeyValueFlat *Self;

        TTxStoreCollect(TKeyValueFlat *keyValueFlat)
            : Self(keyValueFlat)
        {}

        bool Execute(NTabletFlatExecutor::TTransactionContext &txc, const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxStoreCollect Execute");
            TSimpleDbFlat db(txc.DB);
            Self->State.StoreCollectExecute(db, ctx);
            return true;
        }

        void Complete(const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << Self->TabletID()
                    << " TTxStoreCollect Complete");
            Self->State.StoreCollectComplete(ctx);
        }
    };

    struct TTxEraseCollect : public NTabletFlatExecutor::ITransaction {
        TKeyValueFlat *Self;

        TTxEraseCollect(TKeyValueFlat *keyValueFlat)
            : Self(keyValueFlat)
        {}

        bool Execute(NTabletFlatExecutor::TTransactionContext &txc, const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << txc.Tablet << " TTxEraseCollect Execute");
            TSimpleDbFlat db(txc.DB);
            Self->State.EraseCollectExecute(db, ctx);
            return true;
        }

        void Complete(const TActorContext &ctx) override {
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << Self->TabletID()
                    << " TTxEraseCollect Complete");
            Self->State.EraseCollectComplete(ctx);
        }
    };

    TKeyValueState State;
    TDeque<TAutoPtr<IEventHandle>> InitialEventsQueue;
    TActorId CollectorActorId;

    void OnDetach(const TActorContext &ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID() << " OnDetach");
        HandleDie(ctx);
    }

    void OnTabletDead(TEvTablet::TEvTabletDead::TPtr &ev, const TActorContext &ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " OnTabletDead " << ev->Get()->ToString());
        HandleDie(ctx);
    }

    void OnActivateExecutor(const TActorContext &ctx) override {
        Executor()->RegisterExternalTabletCounters(State.TakeTabletCounters());
        State.SetupResourceMetrics(Executor()->GetResourceMetrics());
        ctx.Schedule(TDuration::MilliSeconds(PeriodicRefreshMs), new TEvKeyValue::TEvPeriodicRefresh);
        Execute(new TTxInit(ctx.SelfID, *this), ctx);
    }

    void Enqueue(STFUNC_SIG) override {
        SetActivityType(NKikimrServices::TActivity::KEYVALUE_ACTOR);
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE,
                "KeyValue# " << TabletID()
                << " Enqueue, event type# " << (ui32)ev->GetTypeRewrite()
                << " event# " << (ev->HasEvent() ? ev->GetBase()->ToString().c_str() : "serialized?"));
        InitialEventsQueue.push_back(ev);
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // gRPC

    void Handle(TEvKeyValue::TEvRead::TPtr &ev) {
        State.OnEvReadRequest(ev, TActivationContext::AsActorContext(), Info());
    }

    void Handle(TEvKeyValue::TEvReadRange::TPtr &ev) {
        State.OnEvReadRangeRequest(ev, TActivationContext::AsActorContext(), Info());
    }

    void Handle(TEvKeyValue::TEvExecuteTransaction::TPtr &ev) {
        State.OnEvExecuteTransaction(ev, TActivationContext::AsActorContext(), Info());
    }

    void Handle(TEvKeyValue::TEvGetStorageChannelStatus::TPtr &ev) {
        State.OnEvGetStorageChannelStatus(ev, TActivationContext::AsActorContext(), Info());
    }

    void Handle(TEvKeyValue::TEvAcquireLock::TPtr &ev) {
        State.OnEvAcquireLock(ev, TActivationContext::AsActorContext(), Info());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Online state
    void Handle(TEvKeyValue::TEvEraseCollect::TPtr &ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvEraseCollect " << ev->Get()->ToString());
        State.OnEvEraseCollect(ctx);
        Execute(new TTxEraseCollect(this), ctx);
    }

    void Handle(TEvKeyValue::TEvCollect::TPtr &ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvCollect " << ev->Get()->ToString());
        ui64 perGenerationCounterStepSize = State.OnEvCollect(ctx);
        if (perGenerationCounterStepSize) {
            bool isSpringCleanup = false;
            if (!State.GetIsSpringCleanupDone()) {
                isSpringCleanup = true;
                if (Executor()->Generation() == State.GetCollectOperation()->Header.CollectGeneration) {
                    State.SetIsSpringCleanupDone();
                }
            }
            CollectorActorId = ctx.RegisterWithSameMailbox(CreateKeyValueCollector(ctx.SelfID, State.GetCollectOperation(),
                Info(), Executor()->Generation(), State.GetPerGenerationCounter(), isSpringCleanup));
            State.OnEvCollectDone(perGenerationCounterStepSize, ctx);
        } else {
            LOG_ERROR_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                    << " Handle TEvCollect: PerGenerationCounter overflow prevention restart.");
            Become(&TThis::StateBroken);
            ctx.Send(Tablet(), new TEvents::TEvPoisonPill);
        }
    }

    void Handle(TEvKeyValue::TEvStoreCollect::TPtr &ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvStoreCollect " << ev->Get()->ToString());
        Execute(new TTxStoreCollect(this), ctx);
    }

    void CheckYellowChannels(TRequestStat& stat) {
        IExecutor* executor = Executor();
        if ((stat.YellowMoveChannels || stat.YellowStopChannels) && executor) {
            executor->OnYellowChannels(std::move(stat.YellowMoveChannels), std::move(stat.YellowStopChannels));
        }
    }

    void Handle(TEvKeyValue::TEvIntermediate::TPtr &ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvIntermediate " << ev->Get()->ToString());

        CheckYellowChannels(ev->Get()->Intermediate->Stat);

        State.OnEvIntermediate(*(ev->Get()->Intermediate), ctx);
        Execute(new TTxRequest(std::move(ev->Get()->Intermediate), this), ctx);
    }

    void Handle(TEvKeyValue::TEvNotify::TPtr &ev, const TActorContext &ctx) {
        TEvKeyValue::TEvNotify &event = *ev->Get();
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvNotify " << event.ToString());

        CheckYellowChannels(ev->Get()->Stat);
        State.OnRequestComplete(event.RequestUid, event.Generation, event.Step, ctx, Info(), event.Status, event.Stat);
    }

    void Handle(TEvBlobStorage::TEvCollectGarbageResult::TPtr &ev, const TActorContext &ctx) {
        Y_UNUSED(ev);
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
            << " Handle TEvCollectGarbageResult Cookie# " << ev->Cookie <<  " Marker# KV52");
        if (ev->Cookie == (ui64)TKeyValueState::ECollectCookie::Hard) {
            return;
        }

        if (ev->Cookie == (ui64)TKeyValueState::ECollectCookie::SoftInitial) {
            NKikimrProto::EReplyStatus status = ev->Get()->Status;
            if (status == NKikimrProto::OK) {
                State.RegisterInitialCollectResult(ctx);
            } else {
                LOG_ERROR_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                    << " received not ok TEvCollectGarbageResult"
                    << " Status# " << NKikimrProto::EReplyStatus_Name(status)
                    << " ErrorReason# " << ev->Get()->ErrorReason);
                Send(SelfId(), new TKikimrEvents::TEvPoisonPill);
            }
            return;
        }

        LOG_CRIT_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
            << " received TEvCollectGarbageResult with unexpected Cookie# " << ev->Cookie);
        Send(SelfId(), new TKikimrEvents::TEvPoisonPill);
    }

    void Handle(TEvKeyValue::TEvRequest::TPtr ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " Handle TEvRequest " << ev->Get()->ToString());
        UpdateTabletYellow();
        State.OnEvRequest(ev, ctx, Info());
    }

    void Handle(TEvKeyValue::TEvPeriodicRefresh::TPtr ev, const TActorContext &ctx) {
        Y_UNUSED(ev);
        LOG_TRACE_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID() << " Handle TEvPeriodicRefresh");
        ctx.Schedule(TDuration::MilliSeconds(PeriodicRefreshMs), new TEvKeyValue::TEvPeriodicRefresh);
        State.OnPeriodicRefresh(ctx);
    }

    void Handle(TChannelBalancer::TEvUpdateWeights::TPtr ev, const TActorContext& /*ctx*/) {
        State.OnUpdateWeights(ev);
    }

    bool OnRenderAppHtmlPage(NMon::TEvRemoteHttpInfo::TPtr ev, const TActorContext &ctx) override {
        if (!Executor() || !Executor()->GetStats().IsActive)
            return false;

        if (!ev)
            return true;

        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID() << " Handle TEvRemoteHttpInfo: %s"
                << ev->Get()->Query.data());
        Execute(new TTxMonitoring(ev->Release(), ev->Sender, this), ctx);

        return true;
    }

    void Handle(TEvents::TEvPoisonPill::TPtr &ev, const TActorContext &ctx) {
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID() << " Handle TEvents::TEvPoisonPill");
        Y_UNUSED(ev);
        Become(&TThis::StateBroken);
        ctx.Send(Tablet(), new TEvents::TEvPoisonPill);
    }

    void RestoreActorActivity() {
        SetActivityType(NKikimrServices::TActivity::KEYVALUE_ACTOR);
    }

public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::KEYVALUE_ACTOR;
    }

    TKeyValueFlat(const TActorId &tablet, TTabletStorageInfo *info)
        : TActor(&TThis::StateInit)
        , TTabletExecutedFlat(info, tablet, new NMiniKQL::TMiniKQLFactory)
    {
        TAutoPtr<TTabletCountersBase> counters(
        new TProtobufTabletCounters<
                ESimpleCounters_descriptor,
                ECumulativeCounters_descriptor,
                EPercentileCounters_descriptor,
                ETxTypes_descriptor
            >());
        State.SetupTabletCounters(counters);
        State.Clear();
    }

    virtual void HandleDie(const TActorContext &ctx)
    {
        if (CollectorActorId) {
            ctx.Send(CollectorActorId, new TEvents::TEvPoisonPill);
        }
        State.Terminate(ctx);
        Die(ctx);
    }

    virtual void CreatedHook(const TActorContext &ctx)
    {
        Y_UNUSED(ctx);
    }

    virtual bool HandleHook(STFUNC_SIG)
    {
        Y_UNUSED(ev);
        Y_UNUSED(ctx);
        return false;
    }

    STFUNC(StateInit) {
        RestoreActorActivity();
        LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                << " StateInit flat event type# " << (ui32)ev->GetTypeRewrite()
                << " event# " << (ev->HasEvent() ? ev->GetBase()->ToString().data() : "serialized?"));
        StateInitImpl(ev, ctx);
    }

    STFUNC(StateWork) {
        if (HandleHook(ev, ctx))
            return;
        RestoreActorActivity();
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvKeyValue::TEvRead, Handle);
            hFunc(TEvKeyValue::TEvReadRange, Handle);
            hFunc(TEvKeyValue::TEvExecuteTransaction, Handle);
            hFunc(TEvKeyValue::TEvGetStorageChannelStatus, Handle);
            hFunc(TEvKeyValue::TEvAcquireLock, Handle);

            HFunc(TEvKeyValue::TEvEraseCollect, Handle);
            HFunc(TEvKeyValue::TEvCollect, Handle);
            HFunc(TEvKeyValue::TEvStoreCollect, Handle);
            HFunc(TEvKeyValue::TEvRequest, Handle);
            HFunc(TEvKeyValue::TEvIntermediate, Handle);
            HFunc(TEvKeyValue::TEvNotify, Handle);
            HFunc(TEvKeyValue::TEvPeriodicRefresh, Handle);
            HFunc(TChannelBalancer::TEvUpdateWeights, Handle);
            HFunc(TEvBlobStorage::TEvCollectGarbageResult, Handle);
            HFunc(TEvents::TEvPoisonPill, Handle);

            default:
                if (!HandleDefaultEvents(ev, ctx)) {
                    LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                            << " StateWork unexpected event type# " << (ui32)ev->GetTypeRewrite()
                            << " event# " << (ev->HasEvent() ? ev->GetBase()->ToString().data() : "serialized?"));
                }
                break;
        }
    }

    STFUNC(StateBroken) {
        RestoreActorActivity();
        switch (ev->GetTypeRewrite()) {
            HFunc(TEvTablet::TEvTabletDead, HandleTabletDead)

            default:
                LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                        << " BrokenState unexpected event type# " << (ui32)ev->GetTypeRewrite()
                        << " event# " << (ev->HasEvent() ? ev->GetBase()->ToString().data() : "serialized?"));
                break;
        }
    }

    void InitSchemeComplete(const TActorContext &ctx) {
        Become(&TThis::StateWork);
        State.OnStateWork(ctx);
        UpdateTabletYellow();
        while (!InitialEventsQueue.empty()) {
            TAutoPtr<IEventHandle> &ev = InitialEventsQueue.front();
            LOG_DEBUG_S(ctx, NKikimrServices::KEYVALUE, "KeyValue# " << TabletID()
                    << " Dequeue, event type# " << (ui32)ev->GetTypeRewrite()
                    << " event# " << (ev->HasEvent() ? ev->GetBase()->ToString().data() : "serialized?"));
            ctx.ExecutorThread.Send(ev.Release());
            InitialEventsQueue.pop_front();
        }
        State.OnInitQueueEmpty(ctx);
    }

    void UpdateTabletYellow() {
        if (Executor()) {
            State.SetTabletYellowMove(Executor()->GetStats().IsAnyChannelYellowMove);
            State.SetTabletYellowStop(Executor()->GetStats().IsAnyChannelYellowStop);
        } else {
            State.SetTabletYellowMove(true);
            State.SetTabletYellowStop(true);
        }

    }

    bool ReassignChannelsEnabled() const override {
        return true;
    }
};


}// NKeyValue
}// NKikimr
