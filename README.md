# Constructed-BER-TLV-Encoder-and-Decoder

This project is a Constructed BER-TLV Encoder and Decoder implemented in C.

## Data Structure

The TLV is designed as a linked data structure. The `Tlv_t` structure is defined as follows:

```
typedef struct {	
	uint16_t nTag;    // Tag field with length up to 2 bytes
	uint32_t nLength; // Length field with length up to 3 bytes, indicating up to 65535 bytes in following Value field
	void* pValue;     // Value field pointer
	void* pChild;     // Pointer pointing to the child TLV of next level
	void* pNext;      // Pointer pointing to next TLV on the same level	
} Tlv_t;
```

This structure represents a TLV object as a node pointing to a TLV at the child level and also pointing to the next TLV on the same level. This allows the code to traverse all the TLV nodes when performing "search TLV" and "free TLV" operations.

# Additional Information
For more information on the Constructed BER-TLV specification, please refer to the file "BER-TLV specification key information.docx".
