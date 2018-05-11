#include "Conversion.h"
#include "Hash.h"
#include <string.h>
#include <stdlib.h>

// consts static PROGMEM?
char BASE58_CHARS[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

size_t toHex(const uint8_t * array, size_t arraySize, char * output, size_t outputSize){
    if(outputSize < 2*arraySize){
        return 0;
    }
    memset(output, 0, outputSize);

    for(int i=0; i < arraySize; i++){
        output[2*i] = (array[i] >> 4) + '0';
        if(output[2*i] > '9'){
            output[2*i] += 'a'-'9'-1;
        }

        output[2*i+1] = (array[i] & 0x0F) + '0';
        if(output[2*i+1] > '9'){
            output[2*i+1] += 'a'-'9'-1;
        }
    }
    return 2*arraySize;
}

String toHex(const uint8_t * array, size_t arraySize){
    char * output;
    size_t outputSize = arraySize * 2 + 1;
    output = (char *) malloc(outputSize);

    toHex(array, arraySize, output, outputSize);
    
    String result(output);
    
    memset(output, 0, outputSize);
    free(output);
    
    return result;
}

uint8_t hexToVal(char c){
  if(c >= '0' && c <= '9'){
    return ((uint8_t)(c - '0')) & 0x0F;
  }
  if(c >= 'A' && c <= 'F'){
    return ((uint8_t)(c-'A'+10)) & 0x0F;
  }
  if(c >= 'a' && c <= 'f'){
    return ((uint8_t)(c-'a'+10)) & 0x0F;
  }
  return 0xFF;
}

size_t fromHex(const char hex[], size_t hexLen, byte array[], size_t arraySize){
    memset(array, 0, arraySize);
    if((hexLen % 2 != 0) || (arraySize < hexLen/2)){
        return 0;
    }
    for(int i=0; i<hexLen/2; i++){
        byte v1 = hexToVal(hex[2*i]);
        byte v2 = hexToVal(hex[2*i+1]);
        if((v1 > 0x0F) || (v2 > 0x0F)){ // if invalid char
            memset(array, 0, arraySize);
            return 0;
        }
        array[i] = (v1<<4) | v2;
    }
    return hexLen/2;
}

size_t fromHex(const char hex[], byte array[], size_t arraySize){
    size_t len = strlen(hex);
    return fromHex(hex, len, array, arraySize);
}


size_t toBase58Length(const uint8_t * array, size_t arraySize){
    // Counting leading zeroes
    size_t zeroCount = 0;
    while(zeroCount < arraySize && !array[zeroCount]){
        zeroCount++;
    }

    /*
     *  Encoded string will have maximum length of
     *  len = arraySize * log(58)/log(256) ≈ arraySize * 1.37
     *  and should be rounded to larger value
     *  Limitation: size_t overflow when arraySize > 65536/56 = 1170 bytes
     *  Extra byte due to numerical error appears after 5117 bytes
     */

    size_t size = (arraySize - zeroCount) * 183 / 134 + 1;
    // size_t size = (arraySize - zeroCount) * 137 / 100 + 1;
    return size+zeroCount;
}

size_t toBase58(const uint8_t * array, size_t arraySize, char * output, size_t outputSize){
    // Counting leading zeroes
    size_t zeroCount = 0;
    while(zeroCount < arraySize && !array[zeroCount]){
        zeroCount++;
    }

    // size estimation. 56/41 ≈ log(58)/log(256)
    size_t size = (arraySize - zeroCount) * 183 / 134 + 1;
    // size_t size = (arraySize - zeroCount) * 137 / 100 + 1;
    if(outputSize < size+zeroCount){
        return 0;
    }

    for(int i = 0; i < outputSize; i++){
        output[i] = 0;
    }

    // array copy for manipulations
    size_t bufferSize = arraySize - zeroCount;
    uint8_t buffer[255] = { 0 };
    for(size_t i = zeroCount; i < arraySize; i++){
        buffer[i - zeroCount] = array[i];
    }

    for(size_t j = 0; j < size; j++){
        uint16_t reminder = 0;
        uint16_t temp = 0;
        for(size_t i = 0; i < bufferSize; i++){      
            temp = (reminder * 256 + buffer[i]);
            reminder = temp % 58;
            buffer[i] = temp/58;
        }
        output[size+zeroCount-j-1] = BASE58_CHARS[reminder];
    }
    for (int i = 0; i < zeroCount; i++){
        output[i] = BASE58_CHARS[0];
    }
    if(outputSize > size+zeroCount){
        output[size+zeroCount] = '\0';
    }

    // removing leading zeroes
    // TODO: refactor
    int shift = 0;
    for(int i=zeroCount; i < size+zeroCount; i++){
        if(output[i]==BASE58_CHARS[0]){
            shift++;
        }else{
            break;
        }
    }
    if(shift>0){
        for(int i=zeroCount+shift; i < size+zeroCount; i++){
            output[i-shift] = output[i];
            output[i] = 0;
        }
    }
    return size+zeroCount-shift;
}

size_t toBase58Check(const uint8_t * array, size_t arraySize, char * output, size_t outputSize){
    uint8_t * arr;
    arr = (byte *) malloc(arraySize+4);
    memcpy(arr, array, arraySize);

    uint8_t hash[32];
    doubleSha(arr, arraySize, hash);
    memcpy(arr+arraySize, hash, 4);

    size_t l = toBase58(arr, arraySize+4, output, outputSize);
    memset(arr, 0, arraySize+4); // secret should not stay in RAM
    free(arr);
    return l;
}

// TODO: add zero count, fix wrong length
size_t fromBase58Length(const char * array, size_t arraySize){
    size_t size = arraySize * 361 / 493 + 1;
    return size;
}

// TODO: fix wrong size estimate, use malloc
size_t fromBase58(const char * encoded, size_t encodedSize, uint8_t * output, size_t outputSize){

    size_t size = fromBase58Length(encoded, encodedSize);
    if(outputSize < size){
        return 0;
    }

    // zero everything
    for(size_t i = 0; i < outputSize; i++){
        output[i] = 0;
    }
    uint8_t zeroCount = 0;
    for(int i = 0; i<encodedSize; i++){
        if(encoded[i] == BASE58_CHARS[0]){
            zeroCount++;
        }else{
            break;
        }
    }

    uint16_t val = 0;
    for(size_t i = 0; i < encodedSize; i++){
        char * pch = strchr(BASE58_CHARS, encoded[i]);
        if(pch!=NULL){
            val = pch - BASE58_CHARS;
            if(val == 58){ // end of line '/0'
              return size;
            }
            for(size_t j = 0; j < outputSize; j++){
                uint16_t cur = output[outputSize-j-1]*58;
                cur += val;
                val = cur/256;
                output[outputSize-j-1] = cur%256;
            }
        }else{
            return 0;
        }
    }
    // shifting array
    uint8_t shift = 0;
    for(int i = zeroCount; i < outputSize; i++){
        if(output[i] == 0){
            shift++;
        }else{
            break;
        }
    }
    if(shift > 0){
        for(int i = shift; i < outputSize; i++){
            output[i-shift] = output[i];
            output[i] = 0;
        }
    }

    return outputSize-shift;
}

// TODO: add size check
size_t fromBase58Check(const char * encoded, size_t encodedSize, uint8_t * output, size_t outputSize){
    uint8_t * arr;
    arr = (byte *) malloc(outputSize+4);
    size_t l = fromBase58(encoded, encodedSize, arr, outputSize+4);
    // memcpy(arr, array, arraySize);

    uint8_t hash[32];
    doubleSha(arr, l-4, hash);
    if(memcmp(arr+l-4, hash, 4)!=0){
        return 0;
    }

    memcpy(output, arr, l-4);

    memset(arr, 0, outputSize+4); // secret should not stay in RAM
    free(arr);
    return l-4;
}

/* Integer conversion */

uint64_t littleEndianToInt(byte array[], size_t arraySize){
    uint64_t num = 0;
    for(int i = 0; i < arraySize; i++){
        num <<= 8;
        num += (array[arraySize-i-1] & 0xFF);
    }
    return num;
}

void intToLittleEndian(uint64_t num, byte array[], size_t arraySize){
    for(int i = 0; i < arraySize; i++){
        array[i] = ((num >> (8*i)) & 0xFF);
    }
}

uint64_t bigEndianToInt(byte array[], size_t arraySize){
    uint64_t num = 0;
    for(int i = 0; i < arraySize; i++){
        num <<= 8;
        num += (array[i] & 0xFF);
    }
    return num;
}

void intToBigEndian(uint64_t num, byte array[], size_t arraySize){
    for(int i = 0; i < arraySize; i++){
        array[arraySize-i-1] = ((num >> (8*i)) & 0xFF);
    }
}

/* Stream conversion */
ByteStream::ByteStream(uint8_t * buffer, size_t length){
    len = length;
    buf = (uint8_t *) calloc( length, sizeof(uint8_t));
    memcpy(buf, buffer, length);
}
ByteStream::~ByteStream(void){
    if(len > 0){
        free(buf);
    }
}
int ByteStream::available(){
    return len-cursor;
}
void ByteStream::flush(){
    return;
}
int ByteStream::peek(){
    if(available()){
        uint8_t c =  buf[cursor];
        return c;
    }else{
        return -1;
    }
}
int ByteStream::read(){
    if(available()){
        uint8_t c =  buf[cursor];
        cursor++;
        return c;
    }else{
        return -1;
    }
}
size_t ByteStream::readBytes(uint8_t * buffer, size_t length){
    size_t l;
    if(available() < length){
        length = available();
    }
    memcpy(buffer, buf+cursor, length);
    cursor += length;
    return length;
}
size_t ByteStream::write(uint8_t b){
    return 1;
}
