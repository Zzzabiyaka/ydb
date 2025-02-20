#include "column_families.h"
#include "table_description.h"
#include "table_settings.h"
#include "ydb_convert.h"

#include <ydb/core/base/appdata.h>
#include <ydb/core/engine/mkql_proto.h>
#include <ydb/core/protos/issue_id.pb.h>
#include <ydb/library/yql/public/issue/yql_issue.h>

#include <util/generic/hash.h>

namespace NKikimr {

static NProtoBuf::Timestamp MillisecToProtoTimeStamp(ui64 ms) {
    NProtoBuf::Timestamp timestamp;
    timestamp.set_seconds((i64)(ms / 1000));
    timestamp.set_nanos((i32)((ms % 1000) * 1000000));
    return timestamp;
}

template <typename TYdbProto>
void FillColumnDescriptionImpl(TYdbProto& out,
        NKikimrMiniKQL::TType& splitKeyType, const NKikimrSchemeOp::TTableDescription& in) {

    splitKeyType.SetKind(NKikimrMiniKQL::ETypeKind::Tuple);
    splitKeyType.MutableTuple()->MutableElement()->Reserve(in.KeyColumnIdsSize());
    THashMap<ui32, size_t> columnIdToKeyPos;
    for (size_t keyPos = 0; keyPos < in.KeyColumnIdsSize(); ++keyPos) {
        ui32 colId = in.GetKeyColumnIds(keyPos);
        columnIdToKeyPos[colId] = keyPos;
        splitKeyType.MutableTuple()->AddElement();
    }

    for (const auto& column : in.GetColumns()) {
        NYql::NProto::TypeIds protoType;
        if (!NYql::NProto::TypeIds_Parse(column.GetType(), &protoType)) {
            throw NYql::TErrorException(NKikimrIssues::TIssuesIds::DEFAULT_ERROR)
                << "Got invalid type: " << column.GetType() << " for column: " << column.GetName();
        }

        auto newColumn = out.add_columns();
        newColumn->set_name(column.GetName());
        auto& item = *newColumn->mutable_type()->mutable_optional_type()->mutable_item();
        if (protoType == NYql::NProto::TypeIds::Decimal) {
            auto typeParams = item.mutable_decimal_type();
            // TODO: Change TEvDescribeSchemeResult to return decimal params
            typeParams->set_precision(22);
            typeParams->set_scale(9);
        } else {
            NMiniKQL::ExportPrimitiveTypeToProto(protoType, item);
        }

        if (columnIdToKeyPos.count(column.GetId())) {
            size_t keyPos = columnIdToKeyPos[column.GetId()];
            auto tupleElement = splitKeyType.MutableTuple()->MutableElement(keyPos);
            tupleElement->SetKind(NKikimrMiniKQL::ETypeKind::Optional);
            ConvertYdbTypeToMiniKQLType(item, *tupleElement->MutableOptional()->MutableItem());
        }

        if (column.HasFamilyName()) {
            newColumn->set_family(column.GetFamilyName());
        }
    }

    if (in.HasTTLSettings() && in.GetTTLSettings().HasEnabled()) {
        const auto& inTTL = in.GetTTLSettings().GetEnabled();

        switch (inTTL.GetColumnUnit()) {
        case NKikimrSchemeOp::TTTLSettings::UNIT_AUTO: {
            auto& outTTL = *out.mutable_ttl_settings()->mutable_date_type_column();
            outTTL.set_column_name(inTTL.GetColumnName());
            outTTL.set_expire_after_seconds(inTTL.GetExpireAfterSeconds());
            break;
        }

        case NKikimrSchemeOp::TTTLSettings::UNIT_SECONDS:
        case NKikimrSchemeOp::TTTLSettings::UNIT_MILLISECONDS:
        case NKikimrSchemeOp::TTTLSettings::UNIT_MICROSECONDS:
        case NKikimrSchemeOp::TTTLSettings::UNIT_NANOSECONDS: {
            auto& outTTL = *out.mutable_ttl_settings()->mutable_value_since_unix_epoch();
            outTTL.set_column_name(inTTL.GetColumnName());
            outTTL.set_column_unit(static_cast<Ydb::Table::ValueSinceUnixEpochModeSettings::Unit>(inTTL.GetColumnUnit()));
            outTTL.set_expire_after_seconds(inTTL.GetExpireAfterSeconds());
            break;
        }

        default:
            break;
        }
    }
}

void FillColumnDescription(Ydb::Table::DescribeTableResult& out,
        NKikimrMiniKQL::TType& splitKeyType, const NKikimrSchemeOp::TTableDescription& in) {
    FillColumnDescriptionImpl(out, splitKeyType, in);
}

void FillColumnDescription(Ydb::Table::CreateTableRequest& out,
        NKikimrMiniKQL::TType& splitKeyType, const NKikimrSchemeOp::TTableDescription& in) {
    FillColumnDescriptionImpl(out, splitKeyType, in);
}

bool ExtractColumnTypeId(ui32& outTypeId, const Ydb::Type& inType, Ydb::StatusIds::StatusCode& status, TString& error) {
    ui32 typeId;
    auto itemType = inType.has_optional_type() ? inType.optional_type().item() : inType;
    switch (itemType.type_case()) {
        case Ydb::Type::kTypeId:
            typeId = (ui32)itemType.type_id();
            break;
        case Ydb::Type::kDecimalType: {
            if (itemType.decimal_type().precision() != NScheme::DECIMAL_PRECISION) {
                status = Ydb::StatusIds::BAD_REQUEST;
                error = Sprintf("Bad decimal precision. Only Decimal(%" PRIu32
                                    ",%" PRIu32 ") is supported for table columns",
                                    NScheme::DECIMAL_PRECISION,
                                    NScheme::DECIMAL_SCALE);
                return false;
            }
            if (itemType.decimal_type().scale() != NScheme::DECIMAL_SCALE) {
                status = Ydb::StatusIds::BAD_REQUEST;
                error = Sprintf("Bad decimal scale. Only Decimal(%" PRIu32
                                    ",%" PRIu32 ") is supported for table columns",
                                    NScheme::DECIMAL_PRECISION,
                                    NScheme::DECIMAL_SCALE);
                return false;
            }
            typeId = NYql::NProto::TypeIds::Decimal;
            break;
        }

        default: {
            status = Ydb::StatusIds::BAD_REQUEST;
            error = "Only optional of data types are supported for table columns";
            return false;
        }
    }

    if (!NYql::NProto::TypeIds_IsValid((int)typeId)) {
        status = Ydb::StatusIds::BAD_REQUEST;
        error = TStringBuilder() << "Got invalid typeId: " << (int)typeId;
        return false;
    }

    outTypeId = typeId;
    return true;
}

bool FillColumnDescription(NKikimrSchemeOp::TTableDescription& out,
    const google::protobuf::RepeatedPtrField<Ydb::Table::ColumnMeta>& in, Ydb::StatusIds::StatusCode& status, TString& error) {

    for (const auto& column : in) {
        NKikimrSchemeOp:: TColumnDescription* cd = out.AddColumns();
        cd->SetName(column.name());
        if (!column.type().has_optional_type()) {
            if (!AppData()->FeatureFlags.GetEnableNotNullColumns()) {
                status = Ydb::StatusIds::UNSUPPORTED;
                error = "Not null columns feature is not supported yet";
                return false;
            }

            cd->SetNotNull(true);
        }

        ui32 typeId;
        if (!ExtractColumnTypeId(typeId, column.type(), status, error)) {
            return false;
        }
        cd->SetType(NYql::NProto::TypeIds_Name(NYql::NProto::TypeIds(typeId)));

        if (!column.family().empty()) {
            cd->SetFamilyName(column.family());
        }
    }

    return true;
}

template <typename TYdbProto>
void FillTableBoundaryImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in, const NKikimrMiniKQL::TType& splitKeyType) {

    for (const auto& boundary : in.GetSplitBoundary()) {
        if (boundary.HasSerializedKeyPrefix()) {
            throw NYql::TErrorException(NKikimrIssues::TIssuesIds::DEFAULT_ERROR)
                << "Unexpected serialized response from txProxy";
        } else if (boundary.HasKeyPrefix()) {
            Ydb::TypedValue* ydbValue = nullptr;

            if constexpr (std::is_same<TYdbProto, Ydb::Table::DescribeTableResult>::value) {
                ydbValue = out.add_shard_key_bounds();
            } else if constexpr (std::is_same<TYdbProto, Ydb::Table::CreateTableRequest>::value) {
                ydbValue = out.mutable_partition_at_keys()->add_split_points();
            } else {
                Y_FAIL("Unknown proto type");
            }

            ConvertMiniKQLTypeToYdbType(
                splitKeyType,
                *ydbValue->mutable_type());

            ConvertMiniKQLValueToYdbValue(
                splitKeyType,
                boundary.GetKeyPrefix(),
                *ydbValue->mutable_value());
        } else {
            throw NYql::TErrorException(NKikimrIssues::TIssuesIds::DEFAULT_ERROR)
                << "Got invalid boundary";
        }
    }
}

void FillTableBoundary(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in, const NKikimrMiniKQL::TType& splitKeyType) {
    FillTableBoundaryImpl<Ydb::Table::DescribeTableResult>(out, in, splitKeyType);
}

void FillTableBoundary(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in, const NKikimrMiniKQL::TType& splitKeyType) {
    FillTableBoundaryImpl<Ydb::Table::CreateTableRequest>(out, in, splitKeyType);
}

template <typename TYdbProto>
void FillIndexDescriptionImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    for (const auto& tableIndex : in.GetTableIndexes()) {
        auto index = out.add_indexes();

        index->set_name(tableIndex.GetName());

        *index->mutable_index_columns() = {
            tableIndex.GetKeyColumnNames().begin(),
            tableIndex.GetKeyColumnNames().end()
        };

        *index->mutable_data_columns() = {
            tableIndex.GetDataColumnNames().begin(),
            tableIndex.GetDataColumnNames().end()
        };

        switch (tableIndex.GetType()) {
        case NKikimrSchemeOp::EIndexType::EIndexTypeGlobal:
            *index->mutable_global_index() = Ydb::Table::GlobalIndex();
            break;
        case NKikimrSchemeOp::EIndexType::EIndexTypeGlobalAsync:
            *index->mutable_global_async_index() = Ydb::Table::GlobalAsyncIndex();
            break;
        default:
            break;
        };

        if constexpr (std::is_same<TYdbProto, Ydb::Table::DescribeTableResult>::value) {
            if (tableIndex.GetState() == NKikimrSchemeOp::EIndexState::EIndexStateReady) {
                index->set_status(Ydb::Table::TableIndexDescription::STATUS_READY);
            } else {
                index->set_status(Ydb::Table::TableIndexDescription::STATUS_BUILDING);
            }
            index->set_size_bytes(tableIndex.GetDataSize());
        }
    }
}

void FillIndexDescription(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillIndexDescriptionImpl(out, in);
}

void FillIndexDescription(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillIndexDescriptionImpl(out, in);
}

bool FillIndexDescription(NKikimrSchemeOp::TIndexedTableCreationConfig& out,
    const Ydb::Table::CreateTableRequest& in, Ydb::StatusIds::StatusCode& status, TString& error) {

    auto returnError = [&status, &error](Ydb::StatusIds::StatusCode code, const TString& msg) -> bool {
        status = code;
        error = msg;
        return false;
    };

    for (const auto& index : in.indexes()) {
        auto indexDesc = out.MutableIndexDescription()->Add();

        if (!index.data_columns().empty() && !AppData()->FeatureFlags.GetEnableDataColumnForIndexTable()) {
            return returnError(Ydb::StatusIds::UNSUPPORTED, "Data column feature is not supported yet");
        }

        // common fields
        indexDesc->SetName(index.name());

        for (const auto& col : index.index_columns()) {
            indexDesc->AddKeyColumnNames(col);
        }

        for (const auto& col : index.data_columns()) {
            indexDesc->AddDataColumnNames(col);
        }

        // specific fields
        switch (index.type_case()) {
        case Ydb::Table::TableIndex::kGlobalIndex:
            indexDesc->SetType(NKikimrSchemeOp::EIndexType::EIndexTypeGlobal);
            break;

        case Ydb::Table::TableIndex::kGlobalAsyncIndex:
            if (!AppData()->FeatureFlags.GetEnableAsyncIndexes()) {
                return returnError(Ydb::StatusIds::UNSUPPORTED, "Async indexes are not supported yet");
            }

            indexDesc->SetType(NKikimrSchemeOp::EIndexType::EIndexTypeGlobalAsync);
            break;

        default:
            // pass through
            // TODO: maybe return BAD_REQUEST?
            break;
        }

        //Disabled for a while. Probably we need to allow set this profile to user
/*
        auto indexTableDesc = indexDesc->MutableIndexImplTableDescription();
        if (index.has_global_index() && index.global_index().has_table_profile()) {
            auto indexTableProfile = index.global_index().table_profile();
            // Copy to common table profile to reuse common ApplyTableProfile method
            Ydb::Table::TableProfile profile;
            profile.mutable_partitioning_policy()->CopyFrom(indexTableProfile.partitioning_policy());

            StatusIds::StatusCode code;
            TString error;
            if (!Profiles.ApplyTableProfile(profile, *indexTableDesc, code, error)) {
                NYql::TIssues issues;
                issues.AddIssue(NYql::TIssue(error));
                return Reply(code, issues, ctx);
            }
        }
*/
    }

    return true;
}

void FillTableStats(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TPathDescription& in, bool withPartitionStatistic) {

    auto stats = out.mutable_table_stats();

    if (withPartitionStatistic) {
        for (const auto& tablePartitionStat : in.GetTablePartitionStats()) {
            auto partition = stats->add_partition_stats();
            partition->set_rows_estimate(tablePartitionStat.GetRowCount());
            partition->set_store_size(tablePartitionStat.GetDataSize() + tablePartitionStat.GetIndexSize());
        }
    }

    stats->set_rows_estimate(in.GetTableStats().GetRowCount());
    stats->set_partitions(in.GetTableStats().GetPartCount());

    stats->set_store_size(in.GetTableStats().GetDataSize() + in.GetTableStats().GetIndexSize());
    for (const auto& index : in.GetTable().GetTableIndexes()) {
        stats->set_store_size(stats->store_size() + index.GetDataSize());
    }

    ui64 modificationTimeMs = in.GetTableStats().GetLastUpdateTime();
    if (modificationTimeMs) {
        auto modificationTime = MillisecToProtoTimeStamp(modificationTimeMs);
        stats->mutable_modification_time()->CopyFrom(modificationTime);
    }

    ui64 creationTimeMs = in.GetSelf().GetCreateStep();
    if (creationTimeMs) {
        auto creationTime = MillisecToProtoTimeStamp(creationTimeMs);
        stats->mutable_creation_time()->CopyFrom(creationTime);
    }
}

static bool IsDefaultFamily(const NKikimrSchemeOp::TFamilyDescription& family) {
    if (family.HasId() && family.GetId() == 0) {
        return true; // explicit id 0
    }
    if (!family.HasId() && !family.HasName()) {
        return true; // neither id nor name specified
    }
    return false;
}

template <typename TYdbProto>
void FillStorageSettingsImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    if (!in.HasPartitionConfig()) {
        return;
    }

    const auto& partConfig = in.GetPartitionConfig();
    if (partConfig.ColumnFamiliesSize() == 0) {
        return;
    }

    for (size_t i = 0; i < partConfig.ColumnFamiliesSize(); ++i) {
        const auto& family = partConfig.GetColumnFamilies(i);
        if (IsDefaultFamily(family)) {
            // Default family also specifies some per-table storage settings
            auto* settings = out.mutable_storage_settings();
            settings->set_store_external_blobs(Ydb::FeatureFlag::DISABLED);

            if (family.HasStorageConfig()) {
                if (family.GetStorageConfig().HasSysLog()) {
                    settings->mutable_tablet_commit_log0()->set_media(family.GetStorageConfig().GetSysLog().GetPreferredPoolKind());
                }
                if (family.GetStorageConfig().HasLog()) {
                    settings->mutable_tablet_commit_log1()->set_media(family.GetStorageConfig().GetLog().GetPreferredPoolKind());
                }
                if (family.GetStorageConfig().HasExternal()) {
                    settings->mutable_external()->set_media(family.GetStorageConfig().GetExternal().GetPreferredPoolKind());
                }

                const ui32 externalThreshold = family.GetStorageConfig().GetExternalThreshold();
                if (externalThreshold != 0 && externalThreshold != Max<ui32>()) {
                    settings->set_store_external_blobs(Ydb::FeatureFlag::ENABLED);
                }
            }

            // Check legacy settings for enabled external blobs
            switch (family.GetStorage()) {
                case NKikimrSchemeOp::ColumnStorage1:
                    // default or unset, no legacy external blobs
                    break;
                case NKikimrSchemeOp::ColumnStorage2:
                case NKikimrSchemeOp::ColumnStorage1Ext1:
                case NKikimrSchemeOp::ColumnStorage1Ext2:
                case NKikimrSchemeOp::ColumnStorage2Ext1:
                case NKikimrSchemeOp::ColumnStorage2Ext2:
                case NKikimrSchemeOp::ColumnStorage1Med2Ext2:
                case NKikimrSchemeOp::ColumnStorage2Med2Ext2:
                case NKikimrSchemeOp::ColumnStorageTest_1_2_1k:
                    settings->set_store_external_blobs(Ydb::FeatureFlag::ENABLED);
                    break;
            }

            break;
        }
    }
}

void FillStorageSettings(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillStorageSettingsImpl(out, in);
}

void FillStorageSettings(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillStorageSettingsImpl(out, in);
}

template <typename TYdbProto>
void FillColumnFamiliesImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    if (!in.HasPartitionConfig()) {
        return;
    }

    const auto& partConfig = in.GetPartitionConfig();
    if (partConfig.ColumnFamiliesSize() == 0) {
        return;
    }

    for (size_t i = 0; i < partConfig.ColumnFamiliesSize(); ++i) {
        const auto& family = partConfig.GetColumnFamilies(i);
        auto* r = out.add_column_families();

        if (family.HasName() && !family.GetName().empty()) {
            r->set_name(family.GetName());
        } else if (IsDefaultFamily(family)) {
            r->set_name("default");
        } else if (family.HasId()) {
            r->set_name(TStringBuilder() << "<id: " << family.GetId() << ">");
        } else {
            r->set_name(family.GetName());
        }

        if (family.HasStorageConfig() && family.GetStorageConfig().HasData()) {
            r->mutable_data()->set_media(family.GetStorageConfig().GetData().GetPreferredPoolKind());
        }

        if (family.HasColumnCodec()) {
            switch (family.GetColumnCodec()) {
                case NKikimrSchemeOp::ColumnCodecPlain:
                    r->set_compression(Ydb::Table::ColumnFamily::COMPRESSION_NONE);
                    break;
                case NKikimrSchemeOp::ColumnCodecLZ4:
                    r->set_compression(Ydb::Table::ColumnFamily::COMPRESSION_LZ4);
                    break;
                case NKikimrSchemeOp::ColumnCodecZSTD:
                    break; // FIXME: not supported
            }
        } else if (family.GetCodec() == 1) {
            // Legacy setting, see datashard
            r->set_compression(Ydb::Table::ColumnFamily::COMPRESSION_LZ4);
        } else {
            r->set_compression(Ydb::Table::ColumnFamily::COMPRESSION_NONE);
        }

        // Check legacy settings for permanent in-memory cache
        if (family.GetInMemory() || family.GetColumnCache() == NKikimrSchemeOp::ColumnCacheEver) {
            r->set_keep_in_memory(Ydb::FeatureFlag::ENABLED);
        }
    }
}

void FillColumnFamilies(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillColumnFamiliesImpl(out, in);
}

void FillColumnFamilies(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillColumnFamiliesImpl(out, in);
}

template <typename TYdbProto>
void FillAttributesImpl(TYdbProto& out,
        const NKikimrSchemeOp::TPathDescription& in) {

    if (!in.UserAttributesSize()) {
        return;
    }

    auto& outAttrs = *out.mutable_attributes();
    for (const auto& inAttr : in.GetUserAttributes()) {
        outAttrs[inAttr.GetKey()] = inAttr.GetValue();
    }
}

void FillAttributes(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TPathDescription& in) {
    FillAttributesImpl(out, in);
}

void FillAttributes(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TPathDescription& in) {
    FillAttributesImpl(out, in);
}

template <typename TYdbProto>
static void FillDefaultPartitioningSettings(TYdbProto& out) {
    // (!) We assume that all partitioning methods are disabled by default. But we don't know it for sure.
    auto& outPartSettings = *out.mutable_partitioning_settings();
    outPartSettings.set_partitioning_by_size(Ydb::FeatureFlag::DISABLED);
    outPartSettings.set_partitioning_by_load(Ydb::FeatureFlag::DISABLED);
}

template <typename TYdbProto>
void FillPartitioningSettingsImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    if (!in.HasPartitionConfig()) {
        FillDefaultPartitioningSettings(out);
        return;
    }

    const auto& partConfig = in.GetPartitionConfig();
    if (!partConfig.HasPartitioningPolicy()) {
        FillDefaultPartitioningSettings(out);
        return;
    }

    auto& outPartSettings = *out.mutable_partitioning_settings();
    const auto& inPartPolicy = partConfig.GetPartitioningPolicy();
    if (inPartPolicy.HasSizeToSplit()) {
        if (inPartPolicy.GetSizeToSplit()) {
            outPartSettings.set_partitioning_by_size(Ydb::FeatureFlag::ENABLED);
            outPartSettings.set_partition_size_mb(inPartPolicy.GetSizeToSplit() / (1 << 20));
        } else {
            outPartSettings.set_partitioning_by_size(Ydb::FeatureFlag::DISABLED);
        }
    } else {
        // (!) We assume that partitioning by size is disabled by default. But we don't know it for sure.
        outPartSettings.set_partitioning_by_size(Ydb::FeatureFlag::DISABLED);
    }

    if (inPartPolicy.HasSplitByLoadSettings()) {
        bool enabled = inPartPolicy.GetSplitByLoadSettings().GetEnabled();
        outPartSettings.set_partitioning_by_load(enabled ? Ydb::FeatureFlag::ENABLED : Ydb::FeatureFlag::DISABLED);
    } else {
        // (!) We assume that partitioning by load is disabled by default. But we don't know it for sure.
        outPartSettings.set_partitioning_by_load(Ydb::FeatureFlag::DISABLED);
    }

    if (inPartPolicy.HasMinPartitionsCount() && inPartPolicy.GetMinPartitionsCount()) {
        outPartSettings.set_min_partitions_count(inPartPolicy.GetMinPartitionsCount());
    }

    if (inPartPolicy.HasMaxPartitionsCount() && inPartPolicy.GetMaxPartitionsCount()) {
        outPartSettings.set_max_partitions_count(inPartPolicy.GetMaxPartitionsCount());
    }
}

void FillPartitioningSettings(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillPartitioningSettingsImpl(out, in);
}

void FillPartitioningSettings(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillPartitioningSettingsImpl(out, in);
}

bool CopyExplicitPartitions(NKikimrSchemeOp::TTableDescription& out,
    const Ydb::Table::ExplicitPartitions& in, Ydb::StatusIds::StatusCode& status, TString& error) {

    try {
        for (auto &point : in.split_points()) {
            auto &dst = *out.AddSplitBoundary()->MutableKeyPrefix();
            ConvertYdbValueToMiniKQLValue(point.type(), point.value(), dst);
        }
    } catch (const std::exception &e) {
        status = Ydb::StatusIds::BAD_REQUEST;
        error = TString("cannot convert split points: ") + e.what();
        return false;
    }

    return true;
}

template <typename TYdbProto>
void FillKeyBloomFilterImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    if (!in.HasPartitionConfig()) {
        return;
    }

    const auto& partConfig = in.GetPartitionConfig();
    if (!partConfig.HasEnableFilterByKey()) {
        return;
    }

    if (partConfig.GetEnableFilterByKey()) {
        out.set_key_bloom_filter(Ydb::FeatureFlag::ENABLED);
    } else {
        out.set_key_bloom_filter(Ydb::FeatureFlag::DISABLED);
    }
}

void FillKeyBloomFilter(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillKeyBloomFilterImpl(out, in);
}

void FillKeyBloomFilter(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillKeyBloomFilterImpl(out, in);
}

template <typename TYdbProto>
void FillReadReplicasSettingsImpl(TYdbProto& out,
        const NKikimrSchemeOp::TTableDescription& in) {

    if (!in.HasPartitionConfig()) {
        return;
    }

    const auto& partConfig = in.GetPartitionConfig();
    if (!partConfig.FollowerGroupsSize() && !partConfig.HasCrossDataCenterFollowerCount() && !partConfig.HasFollowerCount()) {
        return;
    }

    if (partConfig.FollowerGroupsSize()) {
        if (partConfig.FollowerGroupsSize() > 1) {
            // Not supported yet
            return;
        }
        const auto& followerGroup = partConfig.GetFollowerGroups(0);
        if (followerGroup.GetFollowerCountPerDataCenter()) {
            out.mutable_read_replicas_settings()->set_per_az_read_replicas_count(followerGroup.GetFollowerCount());
        } else {
            out.mutable_read_replicas_settings()->set_any_az_read_replicas_count(followerGroup.GetFollowerCount());
        }
    } else if (partConfig.HasCrossDataCenterFollowerCount()) {
        out.mutable_read_replicas_settings()->set_per_az_read_replicas_count(partConfig.GetCrossDataCenterFollowerCount());
    } else if (partConfig.HasFollowerCount()) {
        out.mutable_read_replicas_settings()->set_any_az_read_replicas_count(partConfig.GetFollowerCount());
    }
}

void FillReadReplicasSettings(Ydb::Table::DescribeTableResult& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillReadReplicasSettingsImpl(out, in);
}

void FillReadReplicasSettings(Ydb::Table::CreateTableRequest& out,
        const NKikimrSchemeOp::TTableDescription& in) {
    FillReadReplicasSettingsImpl(out, in);
}

bool FillTableDescription(NKikimrSchemeOp::TModifyScheme& out,
        const Ydb::Table::CreateTableRequest& in, Ydb::StatusIds::StatusCode& status, TString& error)
{
    auto& tableDesc = *out.MutableCreateTable();

    if (!FillColumnDescription(tableDesc, in.columns(), status, error)) {
        return false;
    }

    tableDesc.MutableKeyColumnNames()->CopyFrom(in.primary_key());

    TColumnFamilyManager families(tableDesc.MutablePartitionConfig());
    if (in.has_storage_settings() && !families.ApplyStorageSettings(in.storage_settings(), &status, &error)) {
        return false;
    }
    for (const auto& familySettings : in.column_families()) {
        if (!families.ApplyFamilySettings(familySettings, &status, &error)) {
            return false;
        }
    }

    for (auto [key, value] : in.attributes()) {
        auto& attr = *out.MutableAlterUserAttributes()->AddUserAttributes();
        attr.SetKey(key);
        attr.SetValue(value);
    }

    TList<TString> warnings;
    if (!FillCreateTableSettingsDesc(tableDesc, in, status, error, warnings, false)) {
        return false;
    }

    return true;
}

} // namespace NKikimr
