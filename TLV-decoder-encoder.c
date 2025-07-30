
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define TAG_PC_MASK_FIRST_BYTE 0X20 //bit 6 indicate whether the type is primitive or constructed
#define TAG_NUMBER_MASK_FIRST_BYTE 0X1F //bits b5 - b1 of the first byte 
#define LENGTH_MASK_SECOND_BYTE 0X7F //bits b7 - b1 of the SECOND byte 

#define SPARE_BUFFER_SIZE 6
#define MAX_VALUE_BUFFER_SIZE_IN_BYTE 1000
#define MAX_TXN_REF_LEN 125 //case 1:250; case 2:125
//************************************************************
//type define
//************************************************************
typedef unsigned short uint16_t ;
typedef unsigned int   uint32_t;
typedef unsigned char  uint8_t;
typedef int            BOOL;
#define true           1
#define false          0

typedef struct
{
	uint16_t nTag;    //Tag field with length up to 2 bytes
	uint32_t nLength; //Length field with length up to 3 bytes, indicating up to 65535 bytes in following Value field
	void* pValue;     //Value field pointer
	void* pChild;     //pointer pointing to the child TLV of next level
	void* pNext;      //pointer pointing to next TLV on the same level	
} Tlv_t;

BOOL DEBUG_FLAG=false;//debug message switch

//local function
static BOOL IsTagConstructed(uint16_t tag);

//some helper function

//return true--- tag indicates constructed TLV
//       false --- tag indicates primitive TLV
static BOOL IsTagConstructed(uint16_t tag)
{
	BOOL returnValue = false;

	if(((tag&0xFF) & TAG_PC_MASK_FIRST_BYTE) != 0)		
	{
		returnValue = true;
	}	
	return returnValue;
}


uint32_t swapEndian32(uint32_t num)
{
	// Swap endian (big to little) or (little to big)
	uint32_t b0,b1,b2,b3;
	uint32_t res;

	b0 = (num & 0x000000ff) << 24u;
	b1 = (num & 0x0000ff00) << 8u;
	b2 = (num & 0x00ff0000) >> 8u;
	b3 = (num & 0xff000000) >> 24u;

	res = b0 | b1 | b2 | b3;
	printf("\n Input is 0x%X, output is 0x%X", num, res);

	return res;
}

uint16_t swapEndian16(uint16_t num)
{
	// Swap endian (big to little) or (little to big)
	uint16_t b0,b1;
	uint16_t res;

	b0 = (num & 0x00ff) << 8u;
	b1 = (num & 0xff00) >> 8u;

	res = b0 | b1;
	printf("\n Input is 0x%X, output is 0x%X", num, res);

	return res;
}

uint16_t getRealLength(uint32_t nLength, size_t* copyLengthP)
{
	uint16_t realValLength = 0;
	uint8_t tempByte = (nLength)&0xFF;
	if(tempByte<=0x7F)//1 byte Length field, no header
	{
		realValLength = tempByte;
		*copyLengthP = 1;
	}
	else
	{
		switch(tempByte) 
		{
		case 0x81  :
			realValLength = nLength>>8;
			*copyLengthP = 2;
			break;
		case 0x82  :
			realValLength = (uint16_t)(nLength>>8);
			*copyLengthP = 3;
			break; 

		default : 
			printf("Unsupported format in Length field");
			break;
		}
	}
	return realValLength;
}

uint32_t encode_Length(size_t valueLen)
{
	uint32_t returnLen = 0;

	if(valueLen<=0x7F)//1 byte Len field
	{
		returnLen = valueLen;
	}
	else if(valueLen<=0xFF)//2 byte Len field
	{
		returnLen = (valueLen<<8)+0x81;
	}
	else if(valueLen<=0xFFFF)//3 byte Len field
	{
		returnLen = (valueLen<<8)+0x82;
	}

	return returnLen;
}


void printTLV(Tlv_t * tlv)
{
	uint8_t * valueData;
	size_t realValLength = 0, i=0, copyLength=0;

	printf("\n\n tlv->nTag = 0x%X", tlv->nTag);
	printf("\n tlv->nLength = 0x%X", tlv->nLength);

	if(tlv->nLength !=0)
	{
		realValLength = getRealLength(tlv->nLength, &copyLength);
		valueData = (uint8_t *)(tlv->pValue);
		for(i=0; i<realValLength; i++)
		{
			printf("\n\t TLV Value[%ld] = 0x%02X", i, valueData[i]);
		}
	}
}


void transmitterTLV(Tlv_t * tlv, uint8_t * transmitterBuffer)
{
	uint8_t tempTLBuffer[SPARE_BUFFER_SIZE];
	size_t realValLength = 0, i=0, copyLength=0, addLengthCounter=0;

	memset(tempTLBuffer, '\0', sizeof(tempTLBuffer));

	if(((tlv->nTag)&TAG_NUMBER_MASK_FIRST_BYTE)==TAG_NUMBER_MASK_FIRST_BYTE )//bits b5 - b1 of the first byte equal '11111'
	{
		copyLength = 2;//Tag field with 2 bytes
	}
	else
	{
		copyLength = 1;//Tag field with 1 bytes
	}

	memcpy(tempTLBuffer, &(tlv->nTag), copyLength);//put in tag into the buffer
	addLengthCounter += copyLength;	

	//childTlv Length field
	realValLength = getRealLength(tlv->nLength, &copyLength);
	memcpy(tempTLBuffer+addLengthCounter, &(tlv->nLength), copyLength);//put in Length field

	addLengthCounter += copyLength;	

	//copy tlv TAG and Length field into transmitterBuffer	
	memcpy(transmitterBuffer+SPARE_BUFFER_SIZE-addLengthCounter, tempTLBuffer, addLengthCounter);

	//childTlv Value field
	copyLength = realValLength;
	//memcpy(transmitterBuffer+addLengthCounter, tlv->pValue, copyLength);
	addLengthCounter += copyLength;	

	for(i=0; i<(addLengthCounter+SPARE_BUFFER_SIZE); i++)
	{
		printf("\n\t transmitterBuffer Value[%ld] = 0x%02X", i, transmitterBuffer[i]);
	}
}


//************************************************************
//test function implementation
//************************************************************

// Parse the data in buffer to a TLV object. If the buffer contains more than one
// TLV objects at the top level (naming root TLV) in the buffer, only the first root TLV object is parsed.
// If the first root TLV object contains more than one TLV objects at the second or even further down level (naming children TLV) in the buffer,
// all the children TLV of the first root TLV will parsed.
// return: how many bytes have been decoded
size_t TlvParseAllChildrenTLV(const uint8_t* buffer, size_t length, Tlv_t* tlv)
{
	size_t counter = 0;
	static size_t childCounter = 0;
	size_t fistChildIndexInBuffer = 0;
	uint8_t tempByte = 0;
	size_t copyLength = 0;
	BOOL primitiveFlag = true;	
	size_t i=0;//loop index
	uint8_t * valueData = NULL;
	uint32_t realValueLength = 0;
	Tlv_t* tlv_temp_pointer;

	if(tlv != NULL && buffer != NULL)
	{
		//init tlv first
		tlv->nTag = 0;
		tlv->nLength = 0;
		tlv->pValue = NULL;
		tlv->pChild = NULL;
		tlv->pNext = NULL;

		if(DEBUG_FLAG)printf("\n TlvParseAllChildrenTLV(), counter = %ld, length = %ld", counter, length);
		//skip 00 byte before the TLV data
		while(buffer[counter]==0)
		{
			counter++;
		}
		// parse Tag field
		if(buffer[counter]&TAG_PC_MASK_FIRST_BYTE)
		{
			primitiveFlag = false;//this is a contructed TLV object
			printf("\n\n This is a contructed TLV object.");
		}
		else
		{
			printf("\n\n This is a primitive TLV object.");
		}
		if((buffer[counter]&TAG_NUMBER_MASK_FIRST_BYTE)==TAG_NUMBER_MASK_FIRST_BYTE )//bits b5 - b1 of the first byte equal '11111'
		{
			copyLength = 2;//Tag field with 2 bytes
		}
		else
		{
			copyLength = 1;//Tag field with 1 bytes
		}
		memcpy(&tlv->nTag, &buffer[counter], copyLength);//Tag field with 2 bytes
		counter += copyLength;
		printf("\n\t tlv->nTag = 0x%X", tlv->nTag);

		//parse Length field
		tempByte = buffer[counter];
		if(tempByte<=0x7F)//1 byte Length field, no header
		{
			memcpy(&tlv->nLength, &buffer[counter], 1);
			realValueLength = tlv->nLength;
			counter++;
		}
		else
		{
			switch(tempByte) 
			{
			case 0x81  :
			case 0x82  :
				copyLength = tempByte&LENGTH_MASK_SECOND_BYTE;					
				memcpy(&realValueLength, &buffer[counter+1], copyLength);//skip header to copy					
				memcpy(&tlv->nLength, &buffer[counter], copyLength+1);//copy with header
				counter += (copyLength+1);//don't forget header take 1 byte
				break; 

			default : 
				printf("Unsupported format in Length field");
			}
		}
		printf("\n\t tlv->nLength = 0x%X", tlv->nLength);
		if(DEBUG_FLAG)printf("\n\t realValueLength = 0x%X", realValueLength);
		//parse Value field
		if(realValueLength>0)//only if data is not NULL
		{
			copyLength = realValueLength;
			if(primitiveFlag == false)//constructed TLV
			{
				tlv->pChild = malloc( sizeof(Tlv_t) );
				fistChildIndexInBuffer = counter;

				childCounter = TlvParseAllChildrenTLV(&buffer[fistChildIndexInBuffer], copyLength, (Tlv_t*)tlv->pChild);//recursive calling
				if(DEBUG_FLAG)printf("\n fistChildIndexInBuffer=%ld, childCounter=%ld", fistChildIndexInBuffer, childCounter);
			}
			printf("\n");
			tlv->pValue = malloc(realValueLength);				
			memcpy(tlv->pValue, &buffer[counter], copyLength);//Value field with tlv->nLength bytes
			counter += copyLength;
			valueData = (uint8_t *)(tlv->pValue);
			for(i=0; i<copyLength; i++)
			{
				printf("\n\t primitive TLV Value[%ld] = 0x%02X", i, valueData[i]);
			}

			if(primitiveFlag == false)//constructed TLV
			{
				if(DEBUG_FLAG)printf("\n length=%ld, counter=%ld, realValueLength=%d, childCounter=%ld, fistChildIndexInBuffer=%ld", length, counter, realValueLength, childCounter, fistChildIndexInBuffer);
				tlv_temp_pointer = (Tlv_t *)(tlv->pChild);
				//more child TLV data?
				while(realValueLength>childCounter)
				{
					tlv_temp_pointer->pNext = malloc( sizeof(Tlv_t) );
					childCounter += TlvParseAllChildrenTLV(&buffer[fistChildIndexInBuffer+childCounter], (length-fistChildIndexInBuffer-childCounter), (Tlv_t*)tlv_temp_pointer->pNext);//recursive calling						
					tlv_temp_pointer = (Tlv_t*)(tlv_temp_pointer->pNext);
				}
			}
		}
		else
		{
			tlv->pValue = NULL;
		}

	}
	return counter;	
}
// Parse the data in buffer to a TLV object. If the buffer contains more than one
// TLV objects at the same level in the buffer, only the first TLV object is parsed.
// This function should be called before any other TLV function calls.
// return: true-succeed; false-fail
BOOL TlvParse(const uint8_t* buffer, Tlv_t* tlv)
{
	BOOL returnValue = false;
	size_t counter = 0;
	uint8_t tempByte = 0;
	size_t copyLength = 0;
	BOOL primitiveFlag = true;	
	size_t i=0;//loop index
	uint8_t * valueData = NULL;
	uint32_t realValueLength = 0;

	if(tlv != NULL && buffer != NULL)
	{
		//init tlv first
		tlv->nTag = 0;
		tlv->nLength = 0;
		tlv->pValue = NULL;

		//skip 00 byte before the TLV data
		while(buffer[counter]==0)
		{
			counter++;
		}
		// parse Tag field
		if(buffer[counter]&TAG_PC_MASK_FIRST_BYTE)
		{
			primitiveFlag = false;//this is a contructed TLV object
			printf("\n\n This is a contructed TLV object.");
		}
		else
		{
			printf("\n\n This is a primitive TLV object.");
		}
		if((buffer[counter]&TAG_NUMBER_MASK_FIRST_BYTE)==TAG_NUMBER_MASK_FIRST_BYTE )//bits b5 - b1 of the first byte equal '11111'
		{
			copyLength = 2;//Tag field with 2 bytes
		}
		else
		{
			copyLength = 1;//Tag field with 1 bytes
		}
		memcpy(&tlv->nTag, &buffer[counter], copyLength);//Tag field with 2 bytes
		counter += copyLength;
		printf("\n\t tlv->nTag = 0x%X", tlv->nTag);

		//parse Length field
		tempByte = buffer[counter];
		if(tempByte<=0x7F)//1 byte Length field, no header
		{
			memcpy(&tlv->nLength, &buffer[counter], 1);
			realValueLength = tlv->nLength;
			counter++;
		}
		else
		{
			switch(tempByte) 
			{
			case 0x81  :
			case 0x82  :
				copyLength = tempByte&LENGTH_MASK_SECOND_BYTE;					
				memcpy(&realValueLength, &buffer[counter+1], copyLength);//skip header to copy					
				memcpy(&tlv->nLength, &buffer[counter], copyLength+1);//copy with header
				counter += (copyLength+1);//don't forget header take 1 byte
				break; 

			default : 
				printf("Unsupported format in Length field");
			}
		}
		printf("\n\t tlv->nLength = 0x%X", tlv->nLength);
		if(DEBUG_FLAG)printf("\n\t realValueLength = 0x%X", realValueLength);
		//parse Value field
		if(realValueLength>0)//only if data is not NULL
		{
			copyLength = realValueLength;
			if(primitiveFlag == false)//constructed TLV
			{
				tlv->pValue = malloc( sizeof(Tlv_t) );
				TlvParse(&buffer[counter], (Tlv_t*)tlv->pValue);//recursive calling
			}
			else
			{//primitive TLV
				tlv->pValue = malloc(tlv->nLength);				
				memcpy(tlv->pValue, &buffer[counter], copyLength);//Value field with tlv->nLength bytes
				counter += copyLength;
				valueData = (uint8_t *)(tlv->pValue);
				for(i=0; i<copyLength; i++)
				{
					printf("\n\t primitive TLV Value[%ld] = 0x%02X", i, valueData[i]);
				}
			}
		}
		else
		{
			tlv->pValue = NULL;
		}

		returnValue = true;
	}
	return returnValue;	
}


//Search the TLV tag. 
Tlv_t* TlvSearchTagInTree(uint16_t tag, BOOL recursive,Tlv_t* tlv)
{	
	Tlv_t* tlv_p=tlv;
	Tlv_t* tlv_p_c=tlv;
	Tlv_t* returnTLV=NULL;

	printf("\n Come to search Tag 0x%X", tlv->nTag );
	if(tlv->nTag == tag) 
	{
		returnTLV = tlv;
		printf("\n Found Tag 0x%x ", tag);
	}
	else if(recursive == true)
	{
		if(IsTagConstructed(tlv_p->nTag))//constructive TLV?			
		{
			if(tlv_p->pChild!=NULL)//depth-first
			{
				tlv_p_c = (Tlv_t* )tlv_p->pChild;//point to child
				returnTLV = TlvSearchTagInTree(tag, recursive,tlv_p_c);//search child first
			}
			if((tlv_p->pNext!=NULL)&&(returnTLV==NULL))
			{
				tlv_p_c = (Tlv_t* )tlv_p->pNext;
				returnTLV = TlvSearchTagInTree(tag, recursive,tlv_p_c);
			}
		}
		else
		{
			//free next first
			if(tlv_p->pNext!=NULL)
			{
				tlv_p_c = (Tlv_t* )tlv_p->pNext;//point to next
				returnTLV = TlvSearchTagInTree(tag, recursive,tlv_p_c);
			}
		}
	}
	return returnTLV;
}


// Locate a TLV encoded data object in buffer of given length (if recursively in depth-first order)
//
// Parameters:
//   buffer [IN]: The input buffer.
//   length [IN]: The length of input buffer.
//   tag [IN]: tag ID (up to 2 bytes) to find.
//   recursive [IN]: search sub-nodes or not
//   tlv [OUT]: A pointer to the found TLV object.
//
// Return value:
//   TRUE: if the tag is found.
//   FALSE: otherwise
//
// This function only supports Tag ID up to 2 bytes. 
BOOL TlvSearchTag(const uint8_t* buffer, size_t length, uint16_t tag, BOOL recursive, Tlv_t* tlv)
{
	BOOL returnValue = false;
	Tlv_t tlv_obj;
	size_t decodedCounter = 0;
	
	if(DEBUG_FLAG)printf("\n length = %ld", length);
	while((decodedCounter<length)&&(returnValue == false))
	{
		decodedCounter += TlvParseAllChildrenTLV(buffer, length, &tlv_obj);
		if(DEBUG_FLAG)printf("\n decodedCounter = %ld", decodedCounter);
		tlv = TlvSearchTagInTree(tag, recursive,&tlv_obj);
		if(tlv!=NULL)
		{
			returnValue = true;
		}
	}

	return returnValue;
}




// Create a TLV container
//
// Parameters:
//    tlv [IN]: Pointer to an un-initialized TLV structure. Upon return, it is
//              initialized to represent a TLV container with given tag
//    tag [IN]: The tag of TLV container
//    buffer [IN]: The buffer to store entire TLV object
BOOL TlvCreate(Tlv_t* tlv, uint16_t tag, uint8_t* buffer)
{
	tlv->nTag = tag;
	tlv->nLength = 0;//no Value yet.
	tlv->pValue = buffer;
	tlv->pChild = NULL;
	tlv->pNext = NULL;
	return true;
}

// Add a TLV object to the TLV container
BOOL TlvAdd(Tlv_t* tlv, const Tlv_t* childTlv)
{
	uint32_t addLengthCounter = 0;
	size_t copyLength=0;
	size_t realValLength = getRealLength(tlv->nLength, &copyLength);

	uint8_t *bufferPointer = ((uint8_t *)(tlv->pValue)) + realValLength;//offset tlv->nLength bytes to put new child TLV

	// childTlv Tag field
	if((childTlv->nTag)&TAG_PC_MASK_FIRST_BYTE)
	{
		printf("\n This is a constructed child TLV object.");
	}
	if(((childTlv->nTag)&TAG_NUMBER_MASK_FIRST_BYTE)==TAG_NUMBER_MASK_FIRST_BYTE )//bits b5 - b1 of the first byte equal '11111'
	{
		copyLength = 2;//Tag field with 2 bytes
	}
	else
	{
		copyLength = 1;//Tag field with 1 bytes
	}

	memcpy(bufferPointer, &(childTlv->nTag), copyLength);//put in child tag
	addLengthCounter = copyLength;	

	//childTlv Length field
	realValLength = getRealLength(childTlv->nLength, &copyLength);
	memcpy(bufferPointer+addLengthCounter, &(childTlv->nLength), copyLength);//put in child Length field
	addLengthCounter += copyLength;	

	//childTlv Value field
	copyLength = realValLength;
	memcpy(bufferPointer+addLengthCounter, childTlv->pValue, copyLength);
	addLengthCounter += copyLength;	

	//update parent TLV TAG
	tlv->nTag |= TAG_PC_MASK_FIRST_BYTE; //make parent TLV constructed TLV

	//update parent TLV Length
	realValLength = getRealLength(tlv->nLength, &copyLength);	
	if(DEBUG_FLAG)printf("\n parent realValLength=0x%lx, %ld", realValLength, realValLength);	
	realValLength += addLengthCounter;
	if(DEBUG_FLAG)printf("\n parent realValLength=0x%lx, %ld after addLengthCounter", realValLength, realValLength);
	tlv->nLength = encode_Length(realValLength);
	if(DEBUG_FLAG)printf("\n After encode length, tlv->nLength = 0x%x", tlv->nLength);
	return true;
}

// Add TLV data to the TLV container
BOOL TlvAddData(Tlv_t* tlv, uint16_t tag, const uint8_t* value, size_t valueLen)
{
	BOOL returnValue = false;
	if((valueLen!=0)&&(value!=NULL))
	{
		tlv->pChild = NULL;
	    tlv->pNext = NULL;
		tlv->nTag = tag;
		tlv->nLength = encode_Length(valueLen);
		tlv->pValue = malloc(valueLen);				
		memcpy(tlv->pValue, value, valueLen);//Value field with tlv->nLength bytes		
		returnValue = true;
	}
	return returnValue;
}

//Free the TLV object. If it is a constructed TLV object, the contained child object is freed first.
void TlvFree(Tlv_t* tlv)
{	
	Tlv_t* tlv_p=tlv;
	Tlv_t* tlv_p_c=tlv;

	printf("\nCome to free Tag 0x%X", tlv->nTag);
	if(IsTagConstructed(tlv_p->nTag))//constructive TLV?			
	{
		if(tlv_p->pChild!=NULL)
		{
			tlv_p_c = (Tlv_t* )tlv_p->pChild;//point to child
			TlvFree(tlv_p_c);//free child first
		}
		if(tlv_p->pNext!=NULL)
		{
			tlv_p_c = (Tlv_t* )tlv_p->pNext;
			TlvFree(tlv_p_c);
		}
	}
	else
	{
		//free next first
		if(tlv_p->pNext!=NULL)
		{
			tlv_p_c = (Tlv_t* )tlv_p->pNext;//point to next
			TlvFree(tlv_p_c);//free next first
		}
	}
	if(tlv->pValue!=NULL){free(tlv->pValue);}//free primitive TLV
	printf("\nFreed Tag 0x%X", tlv->nTag);
	return;
}



//******************************************************
//DEMO function
//******************************************************
uint8_t tlv1Data[] =
{
	0x70,0x43,0x5F,0x20,0x1A,0x56,0x49,0x53,
	0x41,0x20,0x41,0x43,0x51,0x55,0x49,0x52,
	0x45,0x52,0x20,0x54,0x45,0x53,0x54,0x20,
	0x43,0x41,0x52,0x44,0x20,0x32,0x39,0x57,
	0x11,0x47,0x61,0x73,0x90,0x01,0x01,0x00,
	0x10,0xD1,0x01,0x22,0x01,0x11,0x43,0x87,
	0x80,0x89,0x9F,0x1F,0x10,0x31,0x31,0x34,
	0x33,0x38,0x30,0x30,0x37,0x38,0x30,0x30,
	0x30,0x30,0x30,0x30,0x30,0x90,0x00
};

uint8_t tlv2Data[] =
{
	0x00, 0x00,
	0x70,0x81,0x83,0x90,0x81,0x80,0x6F,0xC4,
	0x63,0xDD,0xD0,0x2A,0x73,0xB3,0x5C,0x84,
	0xDA,0xA7,0x26,0xEE,0x4D,0x3F,0x25,0x32,
	0x66,0x22,0xF1,0xD8,0x2A,0x07,0x48,0x11,
	0xAE,0x2B,0x1B,0x9A,0x67,0xCB,0x58,0xD9,
	0x55,0x73,0x5E,0xE6,0x35,0xD5,0x71,0xF3,
	0x9B,0x5C,0xE0,0xF6,0x4D,0x71,0xAF,0x73,
	0x2D,0x83,0xF3,0x7E,0x2B,0xD5,0x6D,0x67,
	0x22,0x13,0x76,0xC9,0x9B,0x14,0x3B,0x05,
	0x30,0xF2,0xFC,0xEA,0xB2,0xFE,0x63,0x50,
	0xC6,0x2F,0xCE,0xA0,0xC1,0x63,0xE4,0xBD,
	0x84,0xEC,0xB8,0x43,0x42,0xD0,0x5E,0xBF,
	0xB6,0x8F,0x6A,0x9E,0x49,0x96,0xD2,0xCA,
	0xB9,0x63,0x96,0x2E,0x54,0x8A,0x5B,0xEE,
	0xF5,0xEF,0xFF,0xD0,0x19,0x55,0xB9,0x2A,
	0xB5,0x06,0x4B,0xAC,0xB0,0xC8,0xBC,0x3E,
	0x1C,0x40,0x28,0x6D,0xFE,0xFC
};

/*//ohy self define test data
uint8_t tlv2Data[] =
{
0x00, 0x00,
0x70,0x82,0x83,0x01,0x90,0x82,0x80,0x01, 0x6F,0xC4,
0x63,0xDD,0xD0,0x2A,0x73,0xB3,0x5C,0x84,
0xDA,0xA7,0x26,0xEE,0x4D,0x3F,0x25,0x32,
0x66,0x22,0xF1,0xD8,0x2A,0x07,0x48,0x11,
0xAE,0x2B,0x1B,0x9A,0x67,0xCB,0x58,0xD9,
0x55,0x73,0x5E,0xE6,0x35,0xD5,0x71,0xF3,
0x9B,0x5C,0xE0,0xF6,0x4D,0x71,0xAF,0x73,
0x2D,0x83,0xF3,0x7E,0x2B,0xD5,0x6D,0x67,
0x22,0x13,0x76,0xC9,0x9B,0x14,0x3B,0x05,
0x30,0xF2,0xFC,0xEA,0xB2,0xFE,0x63,0x50,
0xC6,0x2F,0xCE,0xA0,0xC1,0x63,0xE4,0xBD,
0x84,0xEC,0xB8,0x43,0x42,0xD0,0x5E,0xBF,
0xB6,0x8F,0x6A,0x9E,0x49,0x96,0xD2,0xCA,
0xB9,0x63,0x96,0x2E,0x54,0x8A,0x5B,0xEE,
0xF5,0xEF,0xFF,0xD0,0x19,0x55,0xB9,0x2A,
0xB5,0x06,0x4B,0xAC,0xB0,0xC8,0xBC,0x3E,
0x1C,0x40,0x28,0x6D,0xFE,0xFC
};
*/
uint8_t tlv3Data[] =
{
	0x00,0x00,
	0x70,0x0E,0x5A,0x08,0x47,0x61,0x73,0x90,
	0x01,0x01,0x00,0x10,0x5F,0x34,0x01,0x01,
	0x90,0x00
};

uint8_t tlv4Data[] =
{
	0x70,0x59,0x60,0x14,0x57,0x12,0x47,0x61,
	0x73,0x90,0x01,0x01,0x00,0x10,0xD1,0x01,
	0x22,0x01,0x11,0x43,0x87,0x80,0x89,0x90,
	0x5F,0x20,0x1A,0x56,0x49,0x53,0x41,0x20,
	0x41,0x43,0x51,0x55,0x49,0x52,0x45,0x52,
	0x20,0x54,0x45,0x53,0x54,0x20,0x43,0x41,
	0x52,0x44,0x20,0x32,0x39,0x57,0x11,0x47,
	0x61,0x73,0x90,0x01,0x01,0x00,0x10,0xD1,
	0x01,0x22,0x01,0x11,0x43,0x87,0x80,0x89,
	0x9F,0x1F,0x10,0x31,0x31,0x34,0x33,0x38,
	0x30,0x30,0x37,0x38,0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0x90,0x00
};

void put_int32_to_char_array(int32_t value, uint8_t * valueArray)
{
	for(int i = 0; i < 4; ++i)
	{
		valueArray[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
	}
}


void put_int16_to_char_array(uint16_t value, uint8_t * valueArray)
{
	valueArray[0] = (uint8_t)(value & 0xFF);
	valueArray[1] = (uint8_t)((value >> 8) & 0xFF);

	return;
}


//DEMO function


int main(int argc, const char * argv[])
{
#ifdef DEMO_EXTRA_CODE
	uint16_t tagSearch = 0x90;//tag to search	
	Tlv_t tlv_decoder_obj[10];
	BOOL recursiveFlag=true;
	size_t index = 0, decodedCounter=0;
	int tlv_decoder_obj_counter=0;
#endif /* DEMO_EXTRA_CODE */
	Tlv_t tlv_decoder_obj_single;

	uint8_t transmitterBuffer[MAX_VALUE_BUFFER_SIZE_IN_BYTE+SPARE_BUFFER_SIZE];
	uint8_t * tlv_encoder_buffer = transmitterBuffer+SPARE_BUFFER_SIZE;
	Tlv_t tlv_encoder_parent;
	Tlv_t tlv_encoder_obj1, tlv_encoder_obj2, tlv_encoder_obj3, tlv_encoder_obj4;
	uint16_t tag_encoder=0;
	uint8_t TxnRef[MAX_TXN_REF_LEN+1];
	int32_t amount;
	uint8_t txnType;
	uint16_t currencyCode;
	uint8_t valueArray[4];
	uint8_t currencyCodeArray[2];

	if(argc>1)
	{
		if((memcmp("debug", argv[1], 5)==0)||(memcmp("DEBUG", argv[1], 5)==0) )
		{
			DEBUG_FLAG = true;
			printf("\n DEBUG turn on");
		}
	}
	//DEBUG_FLAG = true;//ohy debug temp
	memset(transmitterBuffer, '\0', sizeof(transmitterBuffer));
	memset(TxnRef, '\0', sizeof(TxnRef));

	/*********************************************************/
	//decoder DEMO with the given example code
	/*********************************************************/
#ifdef DEMO_EXTRA_CODE
	tlv_decoder_obj_counter=0;
	decodedCounter = 0;
	
	while(decodedCounter<sizeof(tlv4Data))
	{
		decodedCounter += TlvParseAllChildrenTLV(&tlv4Data[decodedCounter], (sizeof(tlv4Data)-decodedCounter), &tlv_decoder_obj[tlv_decoder_obj_counter++]);
		if(DEBUG_FLAG)printf("\n decodedCounter = %d", decodedCounter);
	}

	while( (--tlv_decoder_obj_counter)>=0)
	{
		TlvFree(&tlv_decoder_obj[tlv_decoder_obj_counter]);//free up the tlv space taken by malloc
	}
#endif /* DEMO_EXTRA_CODE */
	/*********************************************************/
	//decoder and search DEMO with the given example code
	/*********************************************************/
#ifdef DEMO_EXTRA_CODE
	for(index=0; index<2; index++)//two case: recursive and non-recursive search
	{
		tagSearch = 0x57;
		printf("\n\n Search Tag 0x%X in %s mode.", tagSearch, (recursiveFlag?"recursive": "non-recursive"));		
		if(TlvSearchTag(&tlv4Data[0], sizeof(tlv4Data), tagSearch, recursiveFlag, &tlv_decoder_obj_single)==true)
		{
			printf("\nFound the search tag 0x%X in %s mode.", tagSearch, (recursiveFlag?"recursive": "non-recursive"));
		}
		else
		{
			printf("\nCan not find the search tag 0x%X in %s mode.", tagSearch, (recursiveFlag?"recursive": "non-recursive"));
		}
		recursiveFlag=false;		
	}	
#endif /* DEMO_EXTRA_CODE */
	/*********************************************************/
	//Encoder and Decoder for the test structure DEMO
	/*********************************************************/
	/*
	Create tags to encode the following data structure into TLV structure:
	typedef struct
	{
	char TxnRef[MAX_TXN_REF_LEN+1]; // Zero terminated string
	int32_t amount;
	uint8_t txnType;
	uint16_t currencyCode;
	} TxnInfo_t;
	*/
	
	//mimic transmitter, the transmitted data is in transmitterBuffer
	//create parent TLV
	tag_encoder = 0x70;
	TlvCreate(&tlv_encoder_parent, tag_encoder, tlv_encoder_buffer);
	if(DEBUG_FLAG){printTLV(&tlv_encoder_parent);}

	//create child TLV
	//TLV 1
	strncpy((char *)TxnRef, "demo string", strlen("demo string")+1);//add 1 for the terminating null
	tag_encoder = 0xC1;//TAG NUMBER 1
	TlvAddData(&tlv_encoder_obj1, tag_encoder, TxnRef, sizeof(TxnRef));
	if(DEBUG_FLAG){printTLV(&tlv_encoder_obj1);}
	//TLV 2
	amount = 0x12345678;
	put_int32_to_char_array(amount, valueArray);
	tag_encoder = 0xC2;//TAG NUMBER 2
	TlvAddData(&tlv_encoder_obj2, tag_encoder, valueArray, sizeof(valueArray));
	if(DEBUG_FLAG){printTLV(&tlv_encoder_obj2);}
	//TLV 3
	txnType = 0xAB;
	tag_encoder = 0xC3;//TAG NUMBER 3
	TlvAddData(&tlv_encoder_obj3, tag_encoder, &txnType, sizeof(txnType));
	if(DEBUG_FLAG){printTLV(&tlv_encoder_obj3);}
	//TLV 4
	currencyCode = 0xCDEF;
	put_int16_to_char_array(currencyCode, currencyCodeArray);
	tag_encoder = 0xC4;//TAG NUMBER 4
	TlvAddData(&tlv_encoder_obj4, tag_encoder, currencyCodeArray, sizeof(currencyCodeArray));
	if(DEBUG_FLAG){printTLV(&tlv_encoder_obj4);}

	//add child TLV into parent TLV
	TlvAdd(&tlv_encoder_parent, &tlv_encoder_obj1);
	TlvAdd(&tlv_encoder_parent, &tlv_encoder_obj2);
	TlvAdd(&tlv_encoder_parent, &tlv_encoder_obj3);
	TlvAdd(&tlv_encoder_parent, &tlv_encoder_obj4);

	if(DEBUG_FLAG){printTLV(&tlv_encoder_parent);}

	printf("\n\n Transmitter/Encoder");
	transmitterTLV(&tlv_encoder_parent, transmitterBuffer);

	//mimic receiver, decode data structure
	printf("\n\n Receiver/Decoder");
	TlvParseAllChildrenTLV(transmitterBuffer, sizeof(transmitterBuffer), &tlv_decoder_obj_single);
	
	TlvFree(&tlv_decoder_obj_single);
	return 0;
}
 