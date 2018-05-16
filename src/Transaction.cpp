#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include "Bitcoin.h"
#include "Hash.h"
#include "Conversion.h"
#include "utility/sha256.h"

TransactionInput::TransactionInput(void){}
TransactionInput::TransactionInput(byte prev_hash[32], uint32_t prev_index, Script script, uint32_t sequence_number){
    memcpy(hash, prev_hash, 32);
    outputIndex = prev_index;
    scriptSig = script;
    sequence = sequence_number;
}
TransactionInput::TransactionInput(byte prev_hash[32], uint32_t prev_index, uint32_t sequence_number, Script script){
    TransactionInput(prev_hash, prev_index, script, sequence_number);
}
TransactionInput::TransactionInput(byte prev_hash[32], uint32_t prev_index){
    Script script;
    uint32_t sequence_number = 0xffffffff;
    TransactionInput(prev_hash, prev_index, script, sequence_number);
}
size_t TransactionInput::parse(Stream &s){
    size_t len = 0;
    len += s.readBytes(hash, 32);
    uint8_t arr[4];
    len += s.readBytes(arr, 4);
    outputIndex = littleEndianToInt(arr, 4);
    len += scriptSig.parse(s);
    len += s.readBytes(arr, 4);
    sequence = littleEndianToInt(arr, 4);
    if((len != 32+4+scriptSig.length()+4) || (scriptSig.length() == 0)){
        return 0;
    }
    return len;
}
size_t TransactionInput::parse(byte raw[], size_t len){
    ByteStream s(raw, len);
    return parse(s);
}
size_t TransactionInput::length(Script script){
    return 32 + 4 + script.length() + 4;
}
size_t TransactionInput::length(){
    return length(scriptSig);
}
size_t TransactionInput::serialize(Stream &s, Script script){
    size_t len = 0;
    s.write(hash, 32);
    len += 32;
    uint8_t arr[4];
    intToLittleEndian(outputIndex, arr, 4);
    s.write(arr, 4);
    len += 4;
    len += script.serialize(s);
    intToLittleEndian(sequence, arr, 4);
    s.write(arr, 4);
    len += 4;
    return len;
}
size_t TransactionInput::serialize(Stream &s){
    return serialize(s, scriptSig);
}
size_t TransactionInput::serialize(uint8_t array[], size_t len, Script script){
    // TODO: refactor with ByteStream
    if(len < length(script)){
        return 0;
    }
    size_t l = 0;
    memcpy(array, hash, 32);
    l += 32;
    intToLittleEndian(outputIndex, array+l, 4);
    l += 4;
    l += script.serialize(array+l, len-l);
    intToLittleEndian(sequence, array+l, 4);
    l += 4;
    return l;
}
size_t TransactionInput::serialize(uint8_t array[], size_t len){
    return serialize(array, len, scriptSig);
}


TransactionOutput::TransactionOutput(void){}
TransactionOutput::TransactionOutput(uint64_t send_amount, Script outputScript){
    amount = send_amount;
    scriptPubKey = outputScript;
}
size_t TransactionOutput::parse(Stream &s){
    size_t len = 0;
    uint8_t arr[8];
    len += s.readBytes(arr, 8);
    amount = littleEndianToInt(arr, 8);
    len += scriptPubKey.parse(s);
    if((len != 8+scriptPubKey.length()) || (scriptPubKey.length() == 0)){
        return 0;
    }
    return len;
}
size_t TransactionOutput::parse(byte raw[], size_t len){
    ByteStream s(raw, len);
    return parse(s);
}
String TransactionOutput::address(bool testnet){
    return scriptPubKey.address(testnet);
}
size_t TransactionOutput::length(){
    return 8+scriptPubKey.length();
}
size_t TransactionOutput::serialize(Stream &s){
    uint8_t arr[8];
    size_t len = 0;
    intToLittleEndian(amount, arr, 8);
    len += 8;
    s.write(arr, 8);
    len += scriptPubKey.serialize(s);
    return len;
}
size_t TransactionOutput::serialize(uint8_t array[], size_t len){
    if(len < length()){
        return 0;
    }
    intToLittleEndian(amount, array, 8);
    size_t l = 8;
    l += scriptPubKey.serialize(array+l, len-l);
    return l;
}


// TODO: copy constructor, = operator
Transaction::Transaction(void){
    inputsNumber = 0;
    outputsNumber = 0;
}
Transaction::~Transaction(void){
    if(inputsNumber > 0){
        free(txIns);
    }
    if(outputsNumber > 0){
        free(txOuts);
    }
}
size_t Transaction::parse(Stream &s){
    if(inputsNumber > 0){
        free(txIns);
    }
    if(outputsNumber > 0){
        free(txOuts);
    }
    size_t len = 0;
    size_t l;
    uint8_t arr[4];
    len += s.readBytes(arr, 4);
    version = littleEndianToInt(arr, 4);
    if(len != 4){
        return 0;
    }

    // check if I can get inputs len (not with available() because of timeout)
    l = s.peek();
    if(l < 0){
        return 0;
    }
    inputsNumber = readVarInt(s);
    len += lenVarInt(inputsNumber);
    txIns = ( TransactionInput * )calloc( inputsNumber, sizeof(TransactionInput) );
    for(int i = 0; i < inputsNumber; i++){
        TransactionInput txIn;
        l = txIn.parse(s);
        txIns[i] = txIn;
        if(l == 0){
            return 0;
        }else{
            len += l;
        }
    }

    l = s.peek();
    if(l < 0){
        return 0;
    }
    outputsNumber = readVarInt(s);
    len += lenVarInt(outputsNumber);
    txOuts = ( TransactionOutput * )calloc( outputsNumber, sizeof(TransactionOutput) );
    for(int i = 0; i < outputsNumber; i++){
        TransactionOutput txOut;
        l = txOut.parse(s);
        txOuts[i] = txOut;
        if(l == 0){
            return 0;
        }else{
            len += l;
        }
    }

    l = s.readBytes(arr, 4);
    if(l != 4){
        return 0;
    }else{
        len += l;
    }
    locktime = littleEndianToInt(arr, 4);
    return len;
}

size_t Transaction::parse(byte raw[], size_t len){
    ByteStream s(raw, len);
    return parse(s);
}
uint8_t Transaction::addInput(TransactionInput txIn){
    inputsNumber ++;
    txIns = ( TransactionInput * )realloc( txIns, inputsNumber * sizeof(TransactionInput) );
    txIns[inputsNumber-1] = txIn;
    return inputsNumber;
}
uint8_t Transaction::addOutput(TransactionOutput txOut){
    outputsNumber ++;
    txOuts = ( TransactionOutput * )realloc( txOuts, outputsNumber * sizeof(TransactionOutput) );
    txOuts[outputsNumber-1] = txOut;
    return outputsNumber;
}
size_t Transaction::length(){
    size_t len = 8 + lenVarInt(inputsNumber) + lenVarInt(outputsNumber); // version + locktime + inputsNumber + outputsNumber
    for(int i=0; i<inputsNumber; i++){
        len += txIns[i].length();
    }
    for(int i=0; i<outputsNumber; i++){
        len += txOuts[i].length();
    }
    return len;
}
size_t Transaction::serialize(Stream &s){
    uint8_t arr[4];
    size_t len = 0;
    intToLittleEndian(version, arr, 4);
    s.write(arr, 4);
    len += 4;
    writeVarInt(inputsNumber, s);
    len += lenVarInt(inputsNumber);
    for(int i=0; i<inputsNumber; i++){
        len += txIns[i].serialize(s);
    }
    writeVarInt(outputsNumber, s);
    len += lenVarInt(outputsNumber);
    for(int i=0; i<outputsNumber; i++){
        len += txOuts[i].serialize(s);
    }
    intToLittleEndian(locktime, arr, 4);
    s.write(arr, 4);
    len += 4;
    return len;
}
size_t Transaction::serialize(uint8_t array[], size_t len){
    if(len < length()){
        return 0;
    }
    size_t l = 0;
    intToLittleEndian(version, array, 4);
    l += 4;
    writeVarInt(inputsNumber, array+l, len-l);
    l += lenVarInt(inputsNumber);
    for(int i=0; i<inputsNumber; i++){
        l += txIns[i].serialize(array+l, len-l);
    }
    writeVarInt(outputsNumber, array+l, len-l);
    l += lenVarInt(outputsNumber);
    for(int i=0; i<outputsNumber; i++){
        l += txOuts[i].serialize(array+l, len-l);
    }
    intToLittleEndian(locktime, array+l, 4);
    l += 4;
    return l;
}

// int Transaction::getHash(int index, PublicKey pubkey, uint8_t hash2[32]){
    // size_t cursor = 0;
    // uint8_t hash[32] = { 0 };
    // struct SHA256_CTX ctx;

    // sha256_init(&ctx);

    // sha256_update(&ctx, raw_data+cursor, 4);
    // cursor += 4; // version
    // inputsNumber = raw_data[cursor] & 0xFF;
    // sha256_update(&ctx, raw_data+cursor, 1);
    // cursor ++;
    // for(int i=0; i<inputsNumber; i++){
    //     sha256_update(&ctx, raw_data+cursor, 32);
    //     cursor += 32; // hash
    //     sha256_update(&ctx, raw_data+cursor, 4);
    //     cursor += 4; // output index
    //     if(index == i){
    //         uint8_t arr[4] = {0x19, 0x76, 0xa9, 0x14};
    //         sha256_update(&ctx, arr, 4);

    //         uint8_t buffer[32];
    //         uint8_t sec_arr[65] = { 0 };
    //         int l = pubkey.sec(sec_arr, sizeof(sec_arr));
    //         hash160(sec_arr, l, buffer);
    //         sha256_update(&ctx, buffer, 20);
    //         uint8_t arr2[2] = {0x88, 0xac};
    //         sha256_update(&ctx, arr2, 2);
    //     }else{
    //         uint8_t arr[1] = { 0x00 };
    //         sha256_update(&ctx, arr, 1);
    //     }
    //     size_t script_len = raw_data[cursor];
    //     cursor ++;
    //     cursor += script_len; // script sig
    //     sha256_update(&ctx, raw_data+cursor, 4);
    //     cursor += 4; // sequence
    // }
    // sha256_update(&ctx, raw_data+cursor, len-cursor);
    // uint8_t sighash_all[4] = { 0x01, 0x00, 0x00, 0x00 };
    // sha256_update(&ctx, sighash_all, 4);

    // sha256_final(&ctx, hash);
    // sha256(hash, 32, hash2);
    // return 0;
// }

// String Transaction::sign(HDPrivateKey key){
    // if(len == 0){
    //     return String("Invalid transaction");
    // }
    // size_t cursor = 0;
    // String result = "";
    // result += toHex(raw_data + cursor, 4);
    // cursor += 4; // version
    // inputsNumber = raw_data[cursor] & 0xFF;
    // result += toHex(raw_data + cursor, 1);
    // cursor ++;
    // for(int i=0; i<inputsNumber; i++){
    //     result += toHex(raw_data + cursor, 32+4);
    //     cursor += 32; // hash
    //     cursor += 4; // output index
    //     size_t script_len = raw_data[cursor];
    //     cursor ++;
    //     if(script_len > 0){
    //         size_t offset = 0;
    //         offset += 5 + 78;
    //         HDPrivateKey myKey = key;
    //         for(int j=0; j<(script_len-offset)/2; j++){
    //             uint16_t der = 0;
    //             der += (raw_data[cursor+offset+2*j+1] & 0xFF);
    //             der *= 8;
    //             der += (raw_data[cursor+offset+2*j] & 0xFF);
    //             myKey = myKey.child(der);
    //         }
    //         PublicKey pubkey = myKey.privateKey.publicKey();
    //         uint8_t hash[32] = { 0 };
    //         getHash(i, pubkey, hash);
    //         Signature sig = myKey.privateKey.sign(hash);
    //         uint8_t der[80] = { 0 };
    //         size_t derLen = sig.der(der, sizeof(der));
    //         der[derLen] = 1;
    //         derLen++;
    //         uint8_t sec[65] = { 0 };
    //         size_t secLen = pubkey.sec(sec, sizeof(sec));
    //         uint8_t lenArr[2] = { secLen + derLen + 2, derLen };
    //         result += toHex(lenArr, 2);
    //         result += toHex(der, derLen);
    //         uint8_t lenArr2[1] = { secLen };
    //         result += toHex(lenArr2, 1);
    //         result += toHex(sec, secLen);
    //         // HDPrivateKey derivedKey
    //     }else{
    //         return String("Unable to sign");
    //     }
    //     cursor += script_len; // script sig
    //     result += toHex(raw_data + cursor, 4);
    //     cursor += 4; // sequence
    // }
    // result += toHex(raw_data + cursor, len-cursor);
    // return result;
// }