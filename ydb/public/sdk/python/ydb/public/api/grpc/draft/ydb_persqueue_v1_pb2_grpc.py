# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

from ydb.public.api.protos import ydb_persqueue_cluster_discovery_pb2 as ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2
from ydb.public.api.protos import ydb_persqueue_v1_pb2 as ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2


class PersQueueServiceStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.StreamingWrite = channel.stream_stream(
                '/Ydb.PersQueue.V1.PersQueueService/StreamingWrite',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteClientMessage.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteServerMessage.FromString,
                )
        self.MigrationStreamingRead = channel.stream_stream(
                '/Ydb.PersQueue.V1.PersQueueService/MigrationStreamingRead',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadClientMessage.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadServerMessage.FromString,
                )
        self.GetReadSessionsInfo = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/GetReadSessionsInfo',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoResponse.FromString,
                )
        self.DescribeTopic = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/DescribeTopic',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicResponse.FromString,
                )
        self.DropTopic = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/DropTopic',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicResponse.FromString,
                )
        self.CreateTopic = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/CreateTopic',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicResponse.FromString,
                )
        self.AlterTopic = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/AlterTopic',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicResponse.FromString,
                )
        self.AddReadRule = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/AddReadRule',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleResponse.FromString,
                )
        self.RemoveReadRule = channel.unary_unary(
                '/Ydb.PersQueue.V1.PersQueueService/RemoveReadRule',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleResponse.FromString,
                )


class PersQueueServiceServicer(object):
    """Missing associated documentation comment in .proto file."""

    def StreamingWrite(self, request_iterator, context):
        """*
        Creates Write Session
        Pipeline:
        client                  server
        Init(Topic, SourceId, ...)
        ---------------->
        Init(Partition, MaxSeqNo, ...)
        <----------------
        write(data1, seqNo1)
        ---------------->
        write(data2, seqNo2)
        ---------------->
        ack(seqNo1, offset1, ...)
        <----------------
        write(data3, seqNo3)
        ---------------->
        ack(seqNo2, offset2, ...)
        <----------------
        issue(description, ...)
        <----------------

        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def MigrationStreamingRead(self, request_iterator, context):
        """*
        Creates Read Session
        Pipeline:
        client                  server
        Init(Topics, ClientId, ...)
        ---------------->
        Init(SessionId)
        <----------------
        read1
        ---------------->
        read2
        ---------------->
        assign(Topic1, Cluster, Partition1, ...) - assigns and releases are optional
        <----------------
        assign(Topic2, Clutster, Partition2, ...)
        <----------------
        start_read(Topic1, Partition1, ...) - client must respond to assign request with this message. Only after this client will start recieving messages from this partition
        ---------------->
        release(Topic1, Partition1, ...)
        <----------------
        released(Topic1, Partition1, ...) - only after released server will give this parittion to other session.
        ---------------->
        start_read(Topic2, Partition2, ...) - client must respond to assign request with this message. Only after this client will start recieving messages from this partition
        ---------------->
        read data(data, ...)
        <----------------
        commit(cookie1)
        ---------------->
        committed(cookie1)
        <----------------
        issue(description, ...)
        <----------------
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def GetReadSessionsInfo(self, request, context):
        """Get information about reading
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def DescribeTopic(self, request, context):
        """
        Describe topic command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def DropTopic(self, request, context):
        """
        Drop topic command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CreateTopic(self, request, context):
        """
        Create topic command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def AlterTopic(self, request, context):
        """
        Alter topic command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def AddReadRule(self, request, context):
        """
        Add read rule command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def RemoveReadRule(self, request, context):
        """
        Remove read rule command.
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_PersQueueServiceServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'StreamingWrite': grpc.stream_stream_rpc_method_handler(
                    servicer.StreamingWrite,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteClientMessage.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteServerMessage.SerializeToString,
            ),
            'MigrationStreamingRead': grpc.stream_stream_rpc_method_handler(
                    servicer.MigrationStreamingRead,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadClientMessage.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadServerMessage.SerializeToString,
            ),
            'GetReadSessionsInfo': grpc.unary_unary_rpc_method_handler(
                    servicer.GetReadSessionsInfo,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoResponse.SerializeToString,
            ),
            'DescribeTopic': grpc.unary_unary_rpc_method_handler(
                    servicer.DescribeTopic,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicResponse.SerializeToString,
            ),
            'DropTopic': grpc.unary_unary_rpc_method_handler(
                    servicer.DropTopic,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicResponse.SerializeToString,
            ),
            'CreateTopic': grpc.unary_unary_rpc_method_handler(
                    servicer.CreateTopic,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicResponse.SerializeToString,
            ),
            'AlterTopic': grpc.unary_unary_rpc_method_handler(
                    servicer.AlterTopic,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicResponse.SerializeToString,
            ),
            'AddReadRule': grpc.unary_unary_rpc_method_handler(
                    servicer.AddReadRule,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleResponse.SerializeToString,
            ),
            'RemoveReadRule': grpc.unary_unary_rpc_method_handler(
                    servicer.RemoveReadRule,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleResponse.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'Ydb.PersQueue.V1.PersQueueService', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class PersQueueService(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def StreamingWrite(request_iterator,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.stream_stream(request_iterator, target, '/Ydb.PersQueue.V1.PersQueueService/StreamingWrite',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteClientMessage.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.StreamingWriteServerMessage.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def MigrationStreamingRead(request_iterator,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.stream_stream(request_iterator, target, '/Ydb.PersQueue.V1.PersQueueService/MigrationStreamingRead',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadClientMessage.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.MigrationStreamingReadServerMessage.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def GetReadSessionsInfo(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/GetReadSessionsInfo',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.ReadInfoResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def DescribeTopic(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/DescribeTopic',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DescribeTopicResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def DropTopic(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/DropTopic',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.DropTopicResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CreateTopic(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/CreateTopic',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.CreateTopicResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def AlterTopic(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/AlterTopic',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AlterTopicResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def AddReadRule(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/AddReadRule',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.AddReadRuleResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def RemoveReadRule(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.PersQueueService/RemoveReadRule',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__v1__pb2.RemoveReadRuleResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)


class ClusterDiscoveryServiceStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.DiscoverClusters = channel.unary_unary(
                '/Ydb.PersQueue.V1.ClusterDiscoveryService/DiscoverClusters',
                request_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersRequest.SerializeToString,
                response_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersResponse.FromString,
                )


class ClusterDiscoveryServiceServicer(object):
    """Missing associated documentation comment in .proto file."""

    def DiscoverClusters(self, request, context):
        """Get PQ clusters which are eligible for the specified Write or Read Sessions
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_ClusterDiscoveryServiceServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'DiscoverClusters': grpc.unary_unary_rpc_method_handler(
                    servicer.DiscoverClusters,
                    request_deserializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersRequest.FromString,
                    response_serializer=ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersResponse.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'Ydb.PersQueue.V1.ClusterDiscoveryService', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class ClusterDiscoveryService(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def DiscoverClusters(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/Ydb.PersQueue.V1.ClusterDiscoveryService/DiscoverClusters',
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersRequest.SerializeToString,
            ydb_dot_public_dot_api_dot_protos_dot_ydb__persqueue__cluster__discovery__pb2.DiscoverClustersResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)
