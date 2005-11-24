/**
 * @file ike_header.c
 * 
 * @brief Declaration of the class ike_header_t. 
 * 
 * An object of this type represents an ike header and is used to 
 * generate and parse ike headers.
 * 
 */

/*
 * Copyright (C) 2005 Jan Hutter, Martin Willi
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/* offsetof macro */
#include <stddef.h>

#include "ike_header.h"

#include <encoding/payloads/encodings.h>
#include <utils/allocator.h>


typedef struct private_ike_header_t private_ike_header_t;

/**
 * Private data of an ike_header_t Object
 * 
 */
struct private_ike_header_t {
	/**
	 * public interface
	 */
	ike_header_t public;
	
	/**
	 * SPI of the initiator
	 */
	u_int64_t initiator_spi;
	/**
	 * SPI of the responder
	 */
	u_int64_t responder_spi;
	/**
	 * next payload type
	 */
	u_int8_t  next_payload;
	/**
	 * IKE major version
	 */
	u_int8_t  maj_version;

	/**
	 * IKE minor version
	 */	
	u_int8_t  min_version;

	/**
	 * Exchange type 
	 */	
	u_int8_t  exchange_type;
	
	/**
	 * Flags of the Message
	 * 
	 */
	struct { 
		/**
		 * Sender is initiator of the associated IKE_SA_INIT-Exchange
		 */
		bool initiator;
		/**
		 * is protocol supporting higher version?
		 */
		bool version;
		/**
		 * TRUE, if this is a response, FALSE if its a Request
		 */
		bool response;
	} flags;
	/**
	 * Associated Message-ID
	 */
	u_int32_t message_id;
	/**
	 * Length of the whole IKEv2-Message (header and all payloads)
	 */
	u_int32_t length;	
}; 

/**
 * mappings used to get strings for exchange_type_t
 */
mapping_t exchange_type_m[] = {
	{EXCHANGE_TYPE_UNDEFINED, "EXCHANGE_TYPE_UNDEFINED"},
	{IKE_SA_INIT, "IKE_SA_INIT"},
	{IKE_AUTH, "IKE_AUTH"},
	{CREATE_CHILD_SA, "CREATE_CHILD_SA"},
	{INFORMATIONAL, "INFORMATIONAL"}
};


/**
 * Encoding rules to parse or generate a IKEv2-Header
 * 
 * The defined offsets are the positions in a object of type 
 * ike_header_t.
 * 
 */
encoding_rule_t ike_header_encodings[] = {
 	/* 8 Byte SPI, stored in the field initiator_spi */
	{ IKE_SPI,		offsetof(private_ike_header_t, initiator_spi)	},
 	/* 8 Byte SPI, stored in the field responder_spi */
	{ IKE_SPI,		offsetof(private_ike_header_t, responder_spi) 	},
 	/* 1 Byte next payload type, stored in the field next_payload */
	{ U_INT_8,		offsetof(private_ike_header_t, next_payload) 	},
 	/* 4 Bit major version, stored in the field maj_version */
	{ U_INT_4,		offsetof(private_ike_header_t, maj_version) 	},
 	/* 4 Bit minor version, stored in the field min_version */
	{ U_INT_4,		offsetof(private_ike_header_t, min_version) 	},
	/* 8 Bit for the exchange type */
	{ U_INT_8,		offsetof(private_ike_header_t, exchange_type) 	},
 	/* 2 Bit reserved bits, nowhere stored */
	{ RESERVED_BIT,	0 										}, 
	{ RESERVED_BIT,	0 										}, 
 	/* 3 Bit flags, stored in the fields response, version and initiator */
	{ FLAG,			offsetof(private_ike_header_t, flags.response) 	},	
	{ FLAG,			offsetof(private_ike_header_t, flags.version) 	},
	{ FLAG,			offsetof(private_ike_header_t, flags.initiator) },
 	/* 3 Bit reserved bits, nowhere stored */
	{ RESERVED_BIT,	0 										},
	{ RESERVED_BIT,	0 										},
	{ RESERVED_BIT,	0 										},
 	/* 4 Byte message id, stored in the field message_id */
	{ U_INT_32,		offsetof(private_ike_header_t, message_id) 		},
 	/* 4 Byte length fied, stored in the field length */
	{ HEADER_LENGTH,	offsetof(private_ike_header_t, length) 			}
};


/*                           1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                       IKE_SA Initiator's SPI                  !
      !                                                               !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                       IKE_SA Responder's SPI                  !
      !                                                               !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !  Next Payload ! MjVer ! MnVer ! Exchange Type !     Flags     !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                          Message ID                           !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                            Length                             !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/


/**
 * Implements payload_t's verify function.
 * See #payload_s.verify for description.
 */
static status_t verify(private_ike_header_t *this)
{
	if ((this->exchange_type < IKE_SA_INIT) || (this->exchange_type > INFORMATIONAL))
	{
		/* unsupported exchange type */
		return FAILED;
	}
	if (this->initiator_spi == 0)
	{
		/* initiator spi not set */
		return FAILED;
	}
	
	if ((this->responder_spi == 0) && (!this->flags.initiator))
	{
		/* must be original initiator*/
		return FAILED;
	}

	if ((this->responder_spi == 0) && (this->flags.response))
	{
		/* must be request*/
		return FAILED;
	}
	
	/* verification of version is not done in here */
	
	return SUCCESS;
}

/**
 * Implements payload_t's set_next_type function.
 * See #payload_s.set_next_type for description.
 */
static status_t set_next_type(payload_t *this,payload_type_t type)
{
	((private_ike_header_t *)this)->next_payload = type;
	return SUCCESS;
}
/**
 * Implements ike_header_t's get_initiator_spi fuction.
 * See #ike_header_t.get_initiator_spi  for description.
 */
static u_int64_t get_initiator_spi(private_ike_header_t *this)
{
	return this->initiator_spi;	
}

/**
 * Implements ike_header_t's set_initiator_spi fuction.
 * See #ike_header_t.set_initiator_spi  for description.
 */
static void set_initiator_spi(private_ike_header_t *this, u_int64_t initiator_spi)
{
	this->initiator_spi = initiator_spi;
}

/**
 * Implements ike_header_t's get_responder_spi fuction.
 * See #ike_header_t.get_responder_spi  for description.
 */
static u_int64_t get_responder_spi(private_ike_header_t *this)
{
	return this->responder_spi;	
}

/**
 * Implements ike_header_t's set_responder_spi fuction.
 * See #ike_header_t.set_responder_spi  for description.
 */
static void set_responder_spi(private_ike_header_t *this, u_int64_t responder_spi)
{
	this->responder_spi = responder_spi;
}

/**
 * Implements ike_header_t's get_maj_version fuction.
 * See #ike_header_t.get_maj_version  for description.
 */
static u_int8_t get_maj_version(private_ike_header_t *this)
{
	return this->maj_version;	
}

/**
 * Implements ike_header_t's get_min_version fuction.
 * See #ike_header_t.get_min_version  for description.
 */
static u_int8_t get_min_version(private_ike_header_t *this)
{
	return this->min_version;	
}

/**
 * Implements ike_header_t's get_response_flag fuction.
 * See #ike_header_t.get_response_flag  for description.
 */
static bool get_response_flag(private_ike_header_t *this)
{
	return this->flags.response;	
}

/**
 * Implements ike_header_t's set_response_flag fuction.
 * See #ike_header_t.set_response_flag  for description.
 */
static void set_response_flag(private_ike_header_t *this, bool response)
{
	this->flags.response = response;	
}

/**
 * Implements ike_header_t's get_version_flag fuction.
 * See #ike_header_t.get_version_flag  for description.
 */
static bool get_version_flag(private_ike_header_t *this)
{
	return this->flags.version;	
}

/**
 * Implements ike_header_t's get_initiator_flag fuction.
 * See #ike_header_t.get_initiator_flag  for description.
 */
static bool get_initiator_flag(private_ike_header_t *this)
{
	return this->flags.initiator;	
}

/**
 * Implements ike_header_t's set_initiator_flag fuction.
 * See #ike_header_t.set_initiator_flag  for description.
 */
static void set_initiator_flag(private_ike_header_t *this, bool initiator)
{
	this->flags.initiator = initiator;	
}

/**
 * Implements ike_header_t's get_exchange_type function
 * See #ike_header_t.get_exchange_type  for description.
 */
static u_int8_t get_exchange_type(private_ike_header_t *this)
{
	return this->exchange_type;	
}

/**
 * Implements ike_header_t's set_exchange_type function.
 * See #ike_header_t.set_exchange_type  for description.
 */
static void set_exchange_type(private_ike_header_t *this, u_int8_t exchange_type)
{
	this->exchange_type = exchange_type;	
}

/**
 * Implements ike_header_t's get_message_id function.
 * See #ike_header_t.get_message_id  for description.
 */
static u_int32_t get_message_id(private_ike_header_t *this)
{
	return this->message_id;	
}

/**
 * Implements ike_header_t's set_message_id function.
 * See #ike_header_t.set_message_id  for description.
 */
static void set_message_id(private_ike_header_t *this, u_int32_t message_id)
{
	this->message_id = message_id;
}

/**
 * Implements payload_t's and ike_header_t's destroy function.
 * See #payload_s.destroy or ike_header_s.destroy for description.
 */
static status_t destroy(ike_header_t *this)
{
	allocator_free(this);
	
	return SUCCESS;
}

/**
 * Implements payload_t's get_encoding_rules function.
 * See #payload_s.get_encoding_rules for description.
 */
static status_t get_encoding_rules(payload_t *this, encoding_rule_t **rules, size_t *rule_count)
{
	*rules = ike_header_encodings;
	*rule_count = sizeof(ike_header_encodings) / sizeof(encoding_rule_t);
	
	return SUCCESS;
}

/**
 * Implements payload_t's get_type function.
 * See #payload_s.get_type for description.
 */
static payload_type_t get_type(payload_t *this)
{
	return HEADER;
}

/**
 * Implements payload_t's get_next_type function.
 * See #payload_s.get_next_type for description.
 */
static payload_type_t get_next_type(payload_t *this)
{
	return (((private_ike_header_t*)this)->next_payload);
}

/**
 * Implements payload_t's get_length function.
 * See #payload_s.get_length for description.
 */
static size_t get_length(payload_t *this)
{
	return (((private_ike_header_t*)this)->length);
}

/*
 * Described in header
 */
ike_header_t *ike_header_create()
{
	private_ike_header_t *this = allocator_alloc_thing(private_ike_header_t);
	if (this == NULL)
	{
		return NULL;	
	}	
	
	this->public.payload_interface.verify = (status_t (*) (payload_t *))verify;
	this->public.payload_interface.get_encoding_rules = get_encoding_rules;
	this->public.payload_interface.get_length = get_length;
	this->public.payload_interface.get_next_type = get_next_type;
	this->public.payload_interface.set_next_type = set_next_type;
	this->public.payload_interface.get_type = get_type;
	this->public.payload_interface.destroy = (status_t (*) (payload_t *))destroy;
	this->public.destroy = destroy;
	
	this->public.get_initiator_spi = (u_int64_t (*) (ike_header_t*))get_initiator_spi;
	this->public.set_initiator_spi = (void (*) (ike_header_t*,u_int64_t))set_initiator_spi;
	this->public.get_responder_spi = (u_int64_t (*) (ike_header_t*))get_responder_spi;
	this->public.set_responder_spi = (void (*) (ike_header_t *,u_int64_t))set_responder_spi;
	this->public.get_maj_version = (u_int8_t (*) (ike_header_t*))get_maj_version;
	this->public.get_min_version = (u_int8_t (*) (ike_header_t*))get_min_version;
	this->public.get_response_flag = (bool (*) (ike_header_t*))get_response_flag;
	this->public.set_response_flag = (void (*) (ike_header_t*,bool))set_response_flag;
	this->public.get_version_flag = (bool (*) (ike_header_t*))get_version_flag;
	this->public.get_initiator_flag = (bool (*) (ike_header_t*))get_initiator_flag;
	this->public.set_initiator_flag = (void (*) (ike_header_t*,bool))set_initiator_flag;
	this->public.get_exchange_type = (u_int8_t (*) (ike_header_t*))get_exchange_type;
	this->public.set_exchange_type = (void (*) (ike_header_t*,u_int8_t))set_exchange_type;
	this->public.get_message_id = (u_int32_t (*) (ike_header_t*))get_message_id;
	this->public.set_message_id = (void (*) (ike_header_t*,u_int32_t))set_message_id;
	
	/* set default values of the fields */
	this->initiator_spi = 0;
	this->responder_spi = 0;
	this->next_payload = 0;
	this->maj_version = IKE_MAJOR_VERSION;
	this->min_version = IKE_MINOR_VERSION;
	this->exchange_type = EXCHANGE_TYPE_UNDEFINED;
	this->flags.initiator = TRUE;
	this->flags.version = HIGHER_VERSION_SUPPORTED_FLAG;
	this->flags.response = FALSE;
	this->message_id = 0;
	this->length = IKE_HEADER_LENGTH;
	
	
	return (ike_header_t*)this;
}


