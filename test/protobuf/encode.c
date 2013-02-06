#include "pb.h"
#include "jansson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){

    json_t *msg, *protos;
    json_error_t error;

    msg = json_load_file("test.json", 0, &error);

    if(!msg){
    	printf("error load msg json\n");
    	return 0;
    }

    protos = json_load_file("test.proto", 0, &error);

    if(!protos){
    	printf("error load protos json\n");
    	return 0;
    }

    uint8_t buffer[512];
    //memset(buffer,0,sizeof(buffer));
    //pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    size_t written = 0;
    if(!pc_pb_encode(buffer,sizeof(buffer),&written,protos,msg)){
    	printf("pb_encode error\n");
    	return 0;
    }

    fwrite(buffer, 1, written, stdout);
    /*int i;
    for(i=0;i<stream.bytes_written;i++){
        printf("%x", buffer[i]);
    }*/	
    printf("\n");
    //printf("%s\n", buffer);
}