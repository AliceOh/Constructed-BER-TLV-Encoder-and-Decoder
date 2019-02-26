# Constructed-BER-TLV-Encoder-and-Decoder
Constructed BER-TLV Enocer and Decoder in C
Basing on the Constructed BER-TLV specification, I design the TLV to be a linked data structure. 
Define Tlv_t as
 typedef struct{	
	uint16_t nTag;    //Tag field with length up to 2 bytes
	uint32_t nLength; //Length field with length up to 3 bytes, indicating up to 65535 bytes in following Value field
	void* pValue;     //Value field pointer
	void* pChild;     //pointer pointing to the child TLV of next level
	void* pNext;      //pointer pointing to next TLV on the same level	
} Tlv_t;
This is to use a linked data structure to represent TLV object as a TLV node pointing to TLV at child level and also pointing to next TLV on the same level.  So when I do “search TLV” and “free TLV”, the code will go through all the TLV nodes.
More information on Constructed BER-TLV specification can refer to the file "BER-TLV specification key information.docx".
