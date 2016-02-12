
#ifndef PROTOBUF_PROTOBUF_C_H
#define PROTOBUF_PROTOBUF_C_H

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
# define PROTOBUF_C__BEGIN_DECLS        extern "C" {
# define PROTOBUF_C__END_DECLS          }
#else
# define PROTOBUF_C__BEGIN_DECLS
# define PROTOBUF_C__END_DECLS
#endif

PROTOBUF_C__BEGIN_DECLS

#ifndef PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE
#define PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(enum_name) \
  , _##enum_name##_IS_INT_SIZE = INT_MAX
#endif

#define PROTOBUF_C__SERVICE_DESCRIPTOR_MAGIC    0x14159bc3
#define PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC    0x28aaeef9
#define PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC       0x114315af

/**
 * \defgroup api Public API
 *
 * This is the public API for `libprotobuf-c`. These interfaces are stable and
 * subject to Semantic Versioning guarantees.
 *
 * @{
 */

/**
 * Values for the `flags` word in `ProtobufCFieldDescriptor`.
 */
typedef enum {
  /** Set if the field is repeated and marked with the `packed` option. */
  PROTOBUF_C_FIELD_FLAG_PACKED            = (1 << 0),

  /** Set if the field is marked with the `deprecated` option. */
  PROTOBUF_C_FIELD_FLAG_DEPRECATED        = (1 << 1),

  /** Set if the field is a member of a oneof (union). */
  PROTOBUF_C_FIELD_FLAG_ONEOF             = (1 << 2),
} ProtobufCFieldFlag;

/**
 * Message field rules.
 *
 * \see [Defining A Message Type] in the Protocol Buffers documentation.
 *
 * [Defining A Message Type]:
 *      https://developers.google.com/protocol-buffers/docs/proto#simple
 */
typedef enum {
  /** A well-formed message must have exactly one of this field. */
  PROTOBUF_C_LABEL_REQUIRED,

  /**
   * A well-formed message can have zero or one of this field (but not
   * more than one).
   */
  PROTOBUF_C_LABEL_OPTIONAL,

  /**
   * This field can be repeated any number of times (including zero) in a
   * well-formed message. The order of the repeated values will be
   * preserved.
   */
  PROTOBUF_C_LABEL_REPEATED,
} ProtobufCLabel;

/**
 * Field value types.
 *
 * \see [Scalar Value Types] in the Protocol Buffers documentation.
 *
 * [Scalar Value Types]:
 *      https://developers.google.com/protocol-buffers/docs/proto#scalar
 */
typedef enum {
  PROTOBUF_C_TYPE_INT32,      /**< int32 */
  PROTOBUF_C_TYPE_SINT32,     /**< signed int32 */
  PROTOBUF_C_TYPE_SFIXED32,   /**< signed int32 (4 bytes) */
  PROTOBUF_C_TYPE_INT64,      /**< int64 */
  PROTOBUF_C_TYPE_SINT64,     /**< signed int64 */
  PROTOBUF_C_TYPE_SFIXED64,   /**< signed int64 (8 bytes) */
  PROTOBUF_C_TYPE_UINT32,     /**< unsigned int32 */
  PROTOBUF_C_TYPE_FIXED32,    /**< unsigned int32 (4 bytes) */
  PROTOBUF_C_TYPE_UINT64,     /**< unsigned int64 */
  PROTOBUF_C_TYPE_FIXED64,    /**< unsigned int64 (8 bytes) */
  PROTOBUF_C_TYPE_FLOAT,      /**< float */
  PROTOBUF_C_TYPE_DOUBLE,     /**< double */
  PROTOBUF_C_TYPE_BOOL,       /**< boolean */
  PROTOBUF_C_TYPE_ENUM,       /**< enumerated type */
  PROTOBUF_C_TYPE_STRING,     /**< UTF-8 or ASCII string */
  PROTOBUF_C_TYPE_BYTES,      /**< arbitrary byte sequence */
  PROTOBUF_C_TYPE_MESSAGE,    /**< nested message */
} ProtobufCType;

/**
 * Field wire types.
 *
 * \see [Message Structure] in the Protocol Buffers documentation.
 *
 * [Message Structure]:
 *      https://developers.google.com/protocol-buffers/docs/encoding#structure
 */
typedef enum {
  PROTOBUF_C_WIRE_TYPE_VARINT = 0,
  PROTOBUF_C_WIRE_TYPE_64BIT = 1,
  PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED = 2,
  /* "Start group" and "end group" wire types are unsupported. */
  PROTOBUF_C_WIRE_TYPE_32BIT = 5,
} ProtobufCWireType;

struct ProtobufCBinaryData;
struct ProtobufCEnumDescriptor;
struct ProtobufCEnumValue;
struct ProtobufCEnumValueIndex;
struct ProtobufCFieldDescriptor;
struct ProtobufCIntRange;
struct ProtobufCMessage;
struct ProtobufCMessageDescriptor;
struct ProtobufCMessageUnknownField;

typedef struct ProtobufCBinaryData ProtobufCBinaryData;
typedef struct ProtobufCEnumDescriptor ProtobufCEnumDescriptor;
typedef struct ProtobufCEnumValue ProtobufCEnumValue;
typedef struct ProtobufCEnumValueIndex ProtobufCEnumValueIndex;
typedef struct ProtobufCFieldDescriptor ProtobufCFieldDescriptor;
typedef struct ProtobufCIntRange ProtobufCIntRange;
typedef struct ProtobufCMessage ProtobufCMessage;
typedef struct ProtobufCMessageDescriptor ProtobufCMessageDescriptor;
typedef struct ProtobufCMessageUnknownField ProtobufCMessageUnknownField;

/** Boolean type. */
typedef int protobuf_c_boolean;

typedef void (*ProtobufCMessageInit)(ProtobufCMessage *);

/**
 * Structure for the protobuf `bytes` scalar type.
 *
 * The data contained in a `ProtobufCBinaryData` is an arbitrary sequence of
 * bytes. It may contain embedded `NUL` characters and is not required to be
 * `NUL`-terminated.
 */
struct ProtobufCBinaryData {
  size_t  len;        /**< Number of bytes in the `data` field. */
  uint8_t *data;      /**< Data bytes. */
};

/**
 * Describes an enumeration as a whole, with all of its values.
 */
struct ProtobufCEnumDescriptor {
  /** Magic value checked to ensure that the API is used correctly. */
  uint32_t                        magic;

  /** The qualified name (e.g., "namespace.Type"). */
  const char                      *name;
  /** The unqualified name as given in the .proto file (e.g., "Type"). */
  const char                      *short_name;
  /** Identifier used in generated C code. */
  const char                      *c_name;
  /** The dot-separated namespace. */
  const char                      *package_name;

  /** Number elements in `values`. */
  unsigned                        n_values;
  /** Array of distinct values, sorted by numeric value. */
  const ProtobufCEnumValue        *values;

  /** Number of elements in `values_by_name`. */
  unsigned                        n_value_names;
  /** Array of named values, including aliases, sorted by name. */
  const ProtobufCEnumValueIndex   *values_by_name;

  /** Number of elements in `value_ranges`. */
  unsigned                        n_value_ranges;
  /** Value ranges, for faster lookups by numeric value. */
  const ProtobufCIntRange         *value_ranges;

  /** Reserved for future use. */
  void                            *reserved1;
  /** Reserved for future use. */
  void                            *reserved2;
  /** Reserved for future use. */
  void                            *reserved3;
  /** Reserved for future use. */
  void                            *reserved4;
};

/**
 * Represents a single value of an enumeration.
 */
struct ProtobufCEnumValue {
  /** The string identifying this value in the .proto file. */
  const char      *name;

  /** The string identifying this value in generated C code. */
  const char      *c_name;

  /** The numeric value assigned in the .proto file. */
  int             value;
};

/**
 * Used by `ProtobufCEnumDescriptor` to look up enum values.
 */
struct ProtobufCEnumValueIndex {
  /** Name of the enum value. */
  const char      *name;
  /** Index into values[] array. */
  unsigned        index;
};

/**
 * Describes a single field in a message.
 */
struct ProtobufCFieldDescriptor {
  /** Name of the field as given in the .proto file. */
  const char              *name;

  /** Tag value of the field as given in the .proto file. */
  uint32_t                id;

  /** Whether the field is `REQUIRED`, `OPTIONAL`, or `REPEATED`. */
  ProtobufCLabel          label;

  /** The type of the field. */
  ProtobufCType           type;

  /**
   * The offset in bytes of the message's C structure's quantifier field
   * (the `has_MEMBER` field for optional members or the `n_MEMBER` field
   * for repeated members or the case enum for oneofs).
   */
  unsigned                quantifier_offset;

  /**
   * The offset in bytes into the message's C structure for the member
   * itself.
   */
  unsigned                offset;

  /**
   * A type-specific descriptor.
   *
   * If `type` is `PROTOBUF_C_TYPE_ENUM`, then `descriptor` points to the
   * corresponding `ProtobufCEnumDescriptor`.
   *
   * If `type` is `PROTOBUF_C_TYPE_MESSAGE`, then `descriptor` points to
   * the corresponding `ProtobufCMessageDescriptor`.
   *
   * Otherwise this field is NULL.
   */
  const void              *descriptor; /* for MESSAGE and ENUM types */

  /** The default value for this field, if defined. May be NULL. */
  const void              *default_value;

  /**
   * A flag word. Zero or more of the bits defined in the
   * `ProtobufCFieldFlag` enum may be set.
   */
  uint32_t                flags;

  /** Reserved for future use. */
  unsigned                reserved_flags;
  /** Reserved for future use. */
  void                    *reserved2;
  /** Reserved for future use. */
  void                    *reserved3;

  size_t elt_size;
  uint32_t max_repeat;
  uint32_t max_length;
};

/**
 * Helper structure for optimizing int => index lookups in the case
 * where the keys are mostly consecutive values, as they presumably are for
 * enums and fields.
 *
 * The data structures requires that the values in the original array are
 * sorted.
 */
struct ProtobufCIntRange {
  int             start_value;
  unsigned        orig_index;
  /*
   * NOTE: the number of values in the range can be inferred by looking
   * at the next element's orig_index. A dummy element is added to make
   * this simple.
   */
};

/**
 * An instance of a message.
 *
 * `ProtobufCMessage` is a light-weight "base class" for all messages.
 *
 * In particular, `ProtobufCMessage` doesn't have any allocation policy
 * associated with it. That's because it's common to create `ProtobufCMessage`
 * objects on the stack. In fact, that's what we recommend for sending messages.
 * If the object is allocated from the stack, you can't really have a memory
 * leak.
 *
 * This means that calls to functions like protobuf_c_message_unpack() which
 * return a `ProtobufCMessage` must be paired with a call to a free function,
 * like protobuf_c_message_free_unpacked().
 */
struct ProtobufCMessage {
  /** The descriptor for this message type. */
  const ProtobufCMessageDescriptor        *descriptor;
  /** The number of elements in `unknown_fields`. */
  unsigned                                n_unknown_fields;
  /** The fields that weren't recognized by the parser. */
  ProtobufCMessageUnknownField            *unknown_fields;
};

/**
 * Describes a message.
 */
struct ProtobufCMessageDescriptor {
  /** Magic value checked to ensure that the API is used correctly. */
  uint32_t                        magic;

  /** The qualified name (e.g., "namespace.Type"). */
  const char                      *name;
  /** The unqualified name as given in the .proto file (e.g., "Type"). */
  const char                      *short_name;
  /** Identifier used in generated C code. */
  const char                      *c_name;
  /** The dot-separated namespace. */
  const char                      *package_name;

  /**
   * Size in bytes of the C structure representing an instance of this
   * type of message.
   */
  size_t                          sizeof_message;

  /** Number of elements in `fields`. */
  unsigned                        n_fields;
  /** Field descriptors, sorted by tag number. */
  const ProtobufCFieldDescriptor  *fields;
  /** Used for looking up fields by name. */
  const unsigned                  *fields_sorted_by_name;

  /** Number of elements in `field_ranges`. */
  unsigned                        n_field_ranges;
  /** Used for looking up fields by id. */
  const ProtobufCIntRange         *field_ranges;

  /** Message initialisation function. */
  ProtobufCMessageInit            message_init;

  /** Reserved for future use. */
  void                            *reserved1;
  /** Reserved for future use. */
  void                            *reserved2;
  /** Reserved for future use. */
  void                            *reserved3;
};

/**
 * An unknown message field.
 */
struct ProtobufCMessageUnknownField {
  /** The tag number. */
  uint32_t                tag;
  /** The wire type of the field. */
  ProtobufCWireType       wire_type;
  /** Number of bytes in `data`. */
  size_t                  len;
  /** Field data. */
  uint8_t                 *data;
};

/**
 * The version of the protobuf-c headers, represented as a string using the same
 * format as protobuf_c_version().
 */
#define PROTOBUF_C_VERSION              "1.1.1"

/**
 * The version of the protobuf-c headers, represented as an integer using the
 * same format as protobuf_c_version_number().
 */
#define PROTOBUF_C_VERSION_NUMBER       1001001

/**
 * The minimum protoc-c version which works with the current version of the
 * protobuf-c headers.
 */
#define PROTOBUF_C_MIN_COMPILER_VERSION 1000000

/**
 * Serialise a message from its in-memory representation.
 *
 * This function stores the serialised bytes of the message in a pre-allocated
 * buffer.
 *
 * \param message
 *      The message object to serialise.
 * \param[out] out
 *      Buffer to store the bytes of the serialised message. This buffer must
 *      have enough space to store the packed message. Use
 *      protobuf_c_message_get_packed_size() to determine the number of bytes
 *      required.
 * \return
 *      Number of bytes stored in `out`.
 */
size_t
protobuf_c_message_pack(const ProtobufCMessage *message, uint8_t *out);

/**
 * Unpack a serialised message into an in-memory representation.
 *
 * \param descriptor
 *      The message descriptor.
 * \param allocator
 *      `ProtobufCAllocator` to use for memory allocation. May be NULL to
 *      specify the default allocator.
 * \param len
 *      Length in bytes of the serialised message.
 * \param data
 *      Pointer to the serialised message.
 * \return
 *      An unpacked message object.
 * \retval NULL
 *      If an error occurred during unpacking.
 */
ProtobufCMessage *
protobuf_c_message_unpack(
    const ProtobufCMessageDescriptor *descriptor,
    size_t len,
    const uint8_t *data,
    ProtobufCMessage *message);

/** Message initialiser. */
#define PROTOBUF_C_MESSAGE_INIT(descriptor) { descriptor, 0, NULL }

/**
 * Initialise a message object from a message descriptor.
 *
 * \param descriptor
 *      Message descriptor.
 * \param message
 *      Allocated block of memory of size `descriptor->sizeof_message`.
 */
void
protobuf_c_message_init(
    const ProtobufCMessageDescriptor *descriptor,
    void *message);

/**@}*/

PROTOBUF_C__END_DECLS

#endif  /* PROTOBUF_PROTOBUF_C_H */
