# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: ydb/public/api/protos/ydb_status_codes.proto
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='ydb/public/api/protos/ydb_status_codes.proto',
  package='Ydb',
  syntax='proto3',
  serialized_options=b'\n\016com.yandex.ydbB\021StatusCodesProtos',
  create_key=_descriptor._internal_create_key,
  serialized_pb=b'\n,ydb/public/api/protos/ydb_status_codes.proto\x12\x03Ydb\"\xa7\x03\n\tStatusIds\"\x99\x03\n\nStatusCode\x12\x1b\n\x17STATUS_CODE_UNSPECIFIED\x10\x00\x12\r\n\x07SUCCESS\x10\x80\xb5\x18\x12\x11\n\x0b\x42\x41\x44_REQUEST\x10\x8a\xb5\x18\x12\x12\n\x0cUNAUTHORIZED\x10\x94\xb5\x18\x12\x14\n\x0eINTERNAL_ERROR\x10\x9e\xb5\x18\x12\r\n\x07\x41\x42ORTED\x10\xa8\xb5\x18\x12\x11\n\x0bUNAVAILABLE\x10\xb2\xb5\x18\x12\x10\n\nOVERLOADED\x10\xbc\xb5\x18\x12\x12\n\x0cSCHEME_ERROR\x10\xc6\xb5\x18\x12\x13\n\rGENERIC_ERROR\x10\xd0\xb5\x18\x12\r\n\x07TIMEOUT\x10\xda\xb5\x18\x12\x11\n\x0b\x42\x41\x44_SESSION\x10\xe4\xb5\x18\x12\x19\n\x13PRECONDITION_FAILED\x10\xf8\xb5\x18\x12\x14\n\x0e\x41LREADY_EXISTS\x10\x82\xb6\x18\x12\x0f\n\tNOT_FOUND\x10\x8c\xb6\x18\x12\x15\n\x0fSESSION_EXPIRED\x10\x96\xb6\x18\x12\x0f\n\tCANCELLED\x10\xa0\xb6\x18\x12\x12\n\x0cUNDETERMINED\x10\xaa\xb6\x18\x12\x11\n\x0bUNSUPPORTED\x10\xb4\xb6\x18\x12\x12\n\x0cSESSION_BUSY\x10\xbe\xb6\x18\x42#\n\x0e\x63om.yandex.ydbB\x11StatusCodesProtosb\x06proto3'
)



_STATUSIDS_STATUSCODE = _descriptor.EnumDescriptor(
  name='StatusCode',
  full_name='Ydb.StatusIds.StatusCode',
  filename=None,
  file=DESCRIPTOR,
  create_key=_descriptor._internal_create_key,
  values=[
    _descriptor.EnumValueDescriptor(
      name='STATUS_CODE_UNSPECIFIED', index=0, number=0,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='SUCCESS', index=1, number=400000,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='BAD_REQUEST', index=2, number=400010,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='UNAUTHORIZED', index=3, number=400020,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='INTERNAL_ERROR', index=4, number=400030,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='ABORTED', index=5, number=400040,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='UNAVAILABLE', index=6, number=400050,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='OVERLOADED', index=7, number=400060,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='SCHEME_ERROR', index=8, number=400070,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='GENERIC_ERROR', index=9, number=400080,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='TIMEOUT', index=10, number=400090,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='BAD_SESSION', index=11, number=400100,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='PRECONDITION_FAILED', index=12, number=400120,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='ALREADY_EXISTS', index=13, number=400130,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='NOT_FOUND', index=14, number=400140,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='SESSION_EXPIRED', index=15, number=400150,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='CANCELLED', index=16, number=400160,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='UNDETERMINED', index=17, number=400170,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='UNSUPPORTED', index=18, number=400180,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
    _descriptor.EnumValueDescriptor(
      name='SESSION_BUSY', index=19, number=400190,
      serialized_options=None,
      type=None,
      create_key=_descriptor._internal_create_key),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=68,
  serialized_end=477,
)
_sym_db.RegisterEnumDescriptor(_STATUSIDS_STATUSCODE)


_STATUSIDS = _descriptor.Descriptor(
  name='StatusIds',
  full_name='Ydb.StatusIds',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
    _STATUSIDS_STATUSCODE,
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=54,
  serialized_end=477,
)

_STATUSIDS_STATUSCODE.containing_type = _STATUSIDS
DESCRIPTOR.message_types_by_name['StatusIds'] = _STATUSIDS
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

StatusIds = _reflection.GeneratedProtocolMessageType('StatusIds', (_message.Message,), {
  'DESCRIPTOR' : _STATUSIDS,
  '__module__' : 'ydb.public.api.protos.ydb_status_codes_pb2'
  # @@protoc_insertion_point(class_scope:Ydb.StatusIds)
  })
_sym_db.RegisterMessage(StatusIds)


DESCRIPTOR._options = None
# @@protoc_insertion_point(module_scope)
