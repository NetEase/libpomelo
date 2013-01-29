/* pb_encode.c -- encode a protobuf using minimal resources
 *
 * 2013 fantasyni <fantasyni@163.com>
 */

//#define PB_INTERNALS
//#define PB_DEBUG
//#define __BIG_ENDIAN__
#include "pb.h"
#include "pb_encode.h"
#include <string.h>

/* The warn_unused_result attribute appeared first in gcc-3.4.0 */
#if !defined(__GNUC__) || ( __GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
    #define checkreturn
#else
    /* Verify that we remember to check all return values for proper error propagation */
    #define checkreturn __attribute__((warn_unused_result))
#endif

/* pb_ostream_t implementation */

static bool checkreturn buf_write(pb_ostream_t *stream, const uint8_t *buf, size_t count){
    uint8_t *dest = (uint8_t*)stream->state;
    memcpy(dest, buf, count);
    stream->state = dest + count;
    return true;
}

pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t bufsize){
    pb_ostream_t stream;
    stream.callback = &buf_write;
    stream.state = buf;
    stream.max_size = bufsize;
    stream.bytes_written = 0;
    return stream;
}

bool checkreturn pb_write(pb_ostream_t *stream, const uint8_t *buf, size_t count){
    if (stream->callback != NULL){
        if (stream->bytes_written + count > stream->max_size)
            return false;
        
        if (!stream->callback(stream, buf, count))
            return false;
    }
    
    stream->bytes_written += count;
    return true;
}

/* Main encoding stuff */

static bool checkreturn pb_encode_array(pb_ostream_t *stream, json_t *protos, 
                             json_t *proto, json_t *array){
     json_t *type = json_object_get(proto,"type");
     const char *type_text = json_string_value(type);
     size_t len = json_array_size(array);
     size_t i;
     // simple msg
     if(pb__get_type(type_text)){
        if (!pb_encode_tag_for_field(stream, proto)){
            return false;
        }
        if (!pb_encode_varint(stream, len)){
            return false;
        }
        for(i=0;i<len;i++){
            if(!pb_encode_proto(stream,protos,proto,json_array_get(array,i))){
                return false;
            }
        }
     }else{
        for(i=0;i<len;i++){
            if (!pb_encode_tag_for_field(stream, proto)){
                return false;
            }
            if(!pb_encode_proto(stream,protos,proto,json_array_get(array,i))){
                return false;
            }
        }
     }
    return true;
}

bool checkreturn pb_encode(pb_ostream_t *stream, json_t *protos, json_t *msg){
    json_t *root,*value,*option,*proto;
    root = msg;

    const char *option_text;
    const char *key;
    void *iter = json_object_iter(root);
    while(iter){
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);
        
        proto = json_object_get(protos,key);
        if(proto){
            option = json_object_get(proto,"option");
            option_text = json_string_value(option);
            if(strcmp(option_text,"required") ==0 || strcmp(option_text,"optional") == 0){
                if (!pb_encode_tag_for_field(stream, proto)){
                    return false;
                }
                    
                if (!pb_encode_proto(stream, protos, proto, value)){
                    return false;
                }
            }else if(strcmp(option_text,"repeated") == 0){
                if(json_is_array(value)){
                    if(!pb_encode_array(stream, protos, proto, value)){
                        return false;
                    }
                }
            }
        }else{
            return false;
        }      
        iter = json_object_iter_next(root, iter);
    }
    return true;
}

bool checkreturn pb_encode_proto(pb_ostream_t *stream,  json_t *protos, json_t *proto, json_t *value){
    json_t *_messages;
    json_t *_type = json_object_get(proto,"type");
    json_t *sub_msg;
    const char *type = json_string_value(_type);
    const char *str;
    json_int_t int_val;
    float float_val;
    double double_val;
    int length;

    _messages = json_object_get(protos,"__messages");
    switch(pb__get_type(type)){
        case PB_uInt32 : 
            int_val = json_number_value(value);
            if(!pb_encode_varint(stream,int_val)){
                return false;
            }
            break;
        case PB_int32 : 
        case PB_sInt32 : 
            int_val = json_number_value(value);
            if(!pb_encode_svarint(stream,int_val)){
                return false;
            }
            break;
        case PB_float : 
            float_val = json_number_value(value);
            if(!pb_encode_fixed32(stream,&float_val)){
                return false;
            }
            break;
        case PB_double : 
            double_val = json_number_value(value);
            if(!pb_encode_fixed64(stream,&double_val)){
                return false;
            }
            break;
        case PB_string : 
            str = json_string_value(value);
            length = strlen(str);
            if(!pb_encode_string(stream, str, length)){
                return false;
            }
            break;
        default : 
            if(_messages){
                sub_msg = json_object_get(_messages,type);
                if(sub_msg){
                    if(!pb_encode_submessage(stream,sub_msg,value)){
                        return false;
                    }
                }else{
                    return false;
                }
            }

    }
    return true;
}

/* Helper functions */
bool checkreturn pb_encode_varint(pb_ostream_t *stream, uint64_t value){
    uint8_t buffer[10];
    size_t i = 0;
    
    if (value == 0)
        return pb_write(stream, (uint8_t*)&value, 1);
    
    while (value){
        buffer[i] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
        i++;
    }
    buffer[i-1] &= 0x7F; /* Unset top bit on last byte */
    
    return pb_write(stream, buffer, i);
}

bool checkreturn pb_encode_svarint(pb_ostream_t *stream, int64_t value){
    uint64_t zigzagged;
    if (value < 0)
        zigzagged = (uint64_t)(~(value << 1));
    else
        zigzagged = (uint64_t)(value << 1);
    
    return pb_encode_varint(stream, zigzagged);
}

bool checkreturn pb_encode_fixed32(pb_ostream_t *stream, const void *value){
    #ifdef __BIG_ENDIAN__
    const uint8_t *bytes = value;
    uint8_t lebytes[4];
    lebytes[0] = bytes[3];
    lebytes[1] = bytes[2];
    lebytes[2] = bytes[1];
    lebytes[3] = bytes[0];
    return pb_write(stream, lebytes, 4);
    #else
    return pb_write(stream, (const uint8_t*)value, 4);
    #endif
}

bool checkreturn pb_encode_fixed64(pb_ostream_t *stream, const void *value){
    #ifdef __BIG_ENDIAN__
    const uint8_t *bytes = value;
    uint8_t lebytes[8];
    lebytes[0] = bytes[7];
    lebytes[1] = bytes[6];
    lebytes[2] = bytes[5];
    lebytes[3] = bytes[4];
    lebytes[4] = bytes[3];
    lebytes[5] = bytes[2];
    lebytes[6] = bytes[1];
    lebytes[7] = bytes[0];
    return pb_write(stream, lebytes, 8);
    #else
    return pb_write(stream, (const uint8_t*)value, 8);
    #endif
}

bool checkreturn pb_encode_tag(pb_ostream_t *stream, int wiretype, uint32_t field_number){
    uint64_t tag = wiretype | (field_number << 3);
    return pb_encode_varint(stream, tag);
}

bool checkreturn pb_encode_tag_for_field(pb_ostream_t *stream, json_t *field){   // type,tag
    int wiretype;
    json_t *type = json_object_get(field,"type");
    json_t *tag = json_object_get(field,"tag");

    wiretype = pb__get_constant_type(json_string_value(type));

    return pb_encode_tag(stream, wiretype, json_number_value(tag));
}

bool checkreturn pb_encode_string(pb_ostream_t *stream, const uint8_t *buffer, size_t size){
    if (!pb_encode_varint(stream, (uint64_t)size))
        return false;
    
    return pb_write(stream, buffer, size);
}

bool checkreturn pb_encode_submessage(pb_ostream_t *stream, json_t *protos, json_t *value){
    /* First calculate the message size using a non-writing substream. */
    pb_ostream_t substream = {0,0,0,0};
    size_t size;
    bool status;

    if (!pb_encode(&substream, protos, value)){
        return false;
    }
    
    size = substream.bytes_written;
    
    if (!pb_encode_varint(stream, (uint64_t)size)){
        return false;
    }

    if (stream->callback == NULL)
        return pb_write(stream, NULL, size); /* Just sizing */
    
    if (stream->bytes_written + size > stream->max_size)
        return false;
        
    /* Use a substream to verify that a callback doesn't write more than
     * what it did the first time. */
    substream.callback = stream->callback;
    substream.state = stream->state;
    substream.max_size = size;
    substream.bytes_written = 0;
    
    status = pb_encode(&substream, protos, value);
    
    stream->bytes_written += substream.bytes_written;
    stream->state = substream.state;
    
    if (substream.bytes_written != size){
        return false;
    }
    
    return status;
}

