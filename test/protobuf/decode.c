#include "pb_encode.h"
#include "pb_decode.h"
#include "jansson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){
	json_t *result,*protos,*msg;
    json_error_t error;
    
    protos = json_load_file("test.proto", 0, &error);
    //printf("%s\n", buffer);
    if(!protos){
    	printf("error load protos json\n");
    	return 0;
    }

    msg = json_load_file("test.json", 0, &error);

    if(!msg){
    	printf("error load msg json\n");
    	return 0;
    }

    uint8_t buffer1[512];
    //memset(buffer,0,sizeof(buffer));
    pb_ostream_t stream1 = pb_ostream_from_buffer(buffer1, sizeof(buffer1));

    if(!pb_encode(&stream1,protos,msg)){
    	printf("pb_encode error\n");
    	return 0;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer1, stream1.bytes_written);
    
    result = json_object();
    if(!pb_decode(&stream,protos,result)){
    	printf("decode error\n");
    	return 0;
    }

    printf("%f\n", json_number_value(json_object_get(result,"a")));
    printf("%s\n\n", json_dumps(result,JSON_INDENT(2)));

    result = json_loads(json_dumps(result,JSON_INDENT(2)),0,&error);
    printf("%f\n", json_number_value(json_object_get(result,"a")));
    
    return 0;
}