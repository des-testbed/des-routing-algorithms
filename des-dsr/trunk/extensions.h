/******************************************************************************
 Copyright 2009, 2010, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
 at Freie Universitaet Berlin (http://www.fu-berlin.de/),
 Computer Systems and Telematics / Distributed, Embedded Systems (DES) group
 (http://cst.mi.fu-berlin.de/, http://www.des-testbed.net/)
 ------------------------------------------------------------------------------
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program. If not, see http://www.gnu.org/licenses/ .
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
 http://www.des-testbed.net/
 ------------------------------------------------------------------------------

 ******************************************************************************/

#ifndef DSR_EXTENSIONS_H_
#define DSR_EXTENSIONS_H_


#define DSR_EXT_RREQ      DESSERT_EXT_USER
#define DSR_EXT_REPL      DESSERT_EXT_USER+1
#define DSR_EXT_RERR      DESSERT_EXT_USER+2
#define DSR_EXT_ACKREQ    DESSERT_EXT_USER+3
#define DSR_EXT_ACK       DESSERT_EXT_USER+4
#define DSR_EXT_SOURCE    DESSERT_EXT_USER+5

#if (METRIC == ETX)
#define DSR_EXT_ETX         DESSERT_EXT_USER+6
#define DSR_EXT_UNICAST_ETX DESSERT_EXT_USER+7
#endif

#define DSR_DONOT_FORWARD_TO_NETWORK_LAYER 0x0001
#define DSR_REPL_EXTENSION_IN_MSG          0x0002
#define DSR_NO_REPL_EXTENSION_IN_MSG       0x0004
#define DSR_RERR_EXTENSION_IN_MSG          0x0008
#define DSR_NO_RERR_EXTENSION_IN_MSG       0x0010


/* used in RREQ and REPL */
typedef struct __attribute__((__packed__)) dsr_hop_data {

    uint8_t address[ETHER_ADDR_LEN];
    uint16_t weight;

} dsr_hop_data_t;


/******************************************************************************
 *
 * RREQ - Route Request
 *
 * "The Route Request option MUST NOT appear more than once within a DSR Options
 *  header." RFC4728 p41
 ******************************************************************************/

/** Initial opt_data_len size without any addresses */
#define DSR_RREQ_INITIAL_OPT_DATA_LEN (sizeof(uint8_t) + sizeof(uint16_t) + ETHER_ADDR_LEN)

/** Length, in octets, of a dsr_rreq_ext excluding data */
#define DSR_RREQ_EXTENSION_HDRLEN (sizeof(uint8_t) + DSR_RREQ_INITIAL_OPT_DATA_LEN)

/** Maximum count of addresses in a dsr_rreq_ext*/
#define DSR_RREQ_MAX_HOPS_IN_OPTION  ((DESSERT_MAXEXTDATALEN - DSR_RREQ_EXTENSION_HDRLEN) / sizeof(dsr_hop_data_t))

#define DSR_RREQ_GET_HOPCOUNT(rreq) ((rreq->opt_data_len - DSR_RREQ_INITIAL_OPT_DATA_LEN)/ sizeof(dsr_hop_data_t))

#define dsr_get_sizeof_rreq(rreq) (rreq->opt_data_len + sizeof(uint8_t))

#define dsr_get_address_begin_in_rreq_by_index(rreq, i) (rreq->data[i].address)

typedef struct __attribute__((__packed__)) dsr_rreq_ext {

    /** Length of the option, in octets, EXCLUDING opt_data_len */
    uint8_t opt_data_len;

    uint8_t ttl;
    uint16_t identification;
    uint8_t target_address[ETHER_ADDR_LEN];

    dsr_hop_data_t data[DSR_RREQ_MAX_HOPS_IN_OPTION];

} dsr_rreq_ext_t;

/******************************************************************************
 *
 * REPL - Route Reply
 *
 * "A Route Reply option MAY appear one or more times within a DSR Options
 *  header." RFC4728 p43
 *
 *  DONE handle multiple occurrences of REPL extension
 ******************************************************************************/

/** Length, in octets, of a dsr_repl_ext excluding address */
#define DSR_REPL_EXTENSION_HDRLEN 1

/** Initial opt_data_len size without any addresses */
#define DSR_REPL_INITIAL_OPT_DATA_LEN (0)

#define DSR_REPL_GET_HOPCOUNT(repl) ((repl->opt_data_len - DSR_REPL_INITIAL_OPT_DATA_LEN)/ sizeof(dsr_hop_data_t))

/** Maximum count of addresses in a dsr_repl_ext*/
#define DSR_REPL_MAX_HOPS_IN_OPTION  ((DESSERT_MAXEXTDATALEN - DSR_REPL_EXTENSION_HDRLEN) / sizeof(dsr_hop_data_t))

//#define DSR_REPL_FLAG_LAST_HOP_EXTERNAL 128

typedef struct __attribute__((__packed__)) dsr_repl_ext {

    /** Because of the DES-DESRT extension mechanism this would be the same
     *  as dessert_ext_t->type */
    //uint8_t    opt_type;

    /** Length of the option, in octets, EXCLUDING opt_data_len.
     *  equals to (ETHER_ADDR_LEN * n) + 1), where n is the
     *  number of addresses in this REPL */
    uint8_t opt_data_len;

    /** Only the leftmost bit is significant (DSR_REPL_FLAG_LAST_HOP_EXTERNAL)
     *  for now. Rest MUST be sent as 0 */
    //uint8_t flags;

    dsr_hop_data_t data[DSR_REPL_MAX_HOPS_IN_OPTION];

} dsr_repl_ext_t;

/******************************************************************************
 *
 * RERR - Route Error
 *
 * "A Route Error option MAY appear one or more times within a DSR  Options
 *  header." RFC4728 p44
 *
 *  DONE handle multiple occurrences of RERR extension
 ******************************************************************************/

#define DSR_RERR_EXTENSION_HDRLEN 19

typedef struct __attribute__((__packed__)) dsr_rerr_ext {

    /** Because of the DES-DESRT extension mechanism this would be the same
     *  as dessert_ext_t->type */
    //uint8_t    opt_type;

    /** Length of the option, in octets, EXCLUDING opt_data_len.
     *
     For the current definition of the Route Error option,
     this field MUST be set to 14, plus the size of any
     Type-Specific Information present in the Route Error.  Further
     extensions to the Route Error option format may also be
     included after the Type-Specific Information portion of the
     Route Error option specified above.  The presence of such
     extensions will be indicated by the Opt Data Len field.
     When the Opt Data Len is greater than that required for
     the fixed portion of the Route Error plus the necessary
     Type-Specific Information as indicated by the Option Type
     value in the option, the remaining octets are interpreted as
     extensions.  Currently, no such further extensions have been
     defined. */
    //uint8_t opt_data_len;

    /**  The type of error encountered.  Currently, the following type
     values are defined:

     1 = NODE_UNREACHABLE
     2 = FLOW_STATE_NOT_SUPPORTED
     3 = OPTION_NOT_SUPPORTED

     Other values of the Error Type field are reserved for future
     use. */
    //uint8_t error_type;

    /** A 4-bit unsigned integer.  Copied from the Salvage field in the
     DSR Source Route option of the packet triggering the Route
     Error.

     The "total salvage count" of the Route Error option is derived
     from the value in the Salvage field of this Route Error option
     and all preceding Route Error options in the packet as follows:
     the total salvage count is the sum of, for each such Route
     Error option, one plus the value in the Salvage field of that
     Route Error option. */
    uint8_t salvage;

    /** The address of the node originating the Route Error (e.g., the
     node that attempted to forward a packet and discovered the link
     failure). */
    uint8_t error_source_address[ETHER_ADDR_LEN];

    /** The address of the node to which the Route Error must be
     delivered.  For example, when the Error Type field is set to
     NODE_UNREACHABLE, this field will be set to the address of the
     node that generated the routing information claiming that the
     hop from the Error Source Address to Unreachable Node Address
     (specified in the Type-Specific Information) was a valid hop. */
    uint8_t error_destination_address[ETHER_ADDR_LEN];

    /** Information specific to the Error Type of this Route Error
     message. */
    uint8_t type_specific_information[6]; //FIXME: see rfc4728 pp45+46

} dsr_rerr_ext_t;

/******************************************************************************
 *
 * ACK REQ - Acknowledgement Request
 *
 * "An Acknowledgement Request option MUST NOT appear more than once within a
 *  DSR Options header." RFC4728 p47
 ******************************************************************************/
#define DSR_ACKREQ_EXTENSION_HDRLEN 2

typedef struct __attribute__((__packed__)) dsr_ackreq_ext {

    /** 8-bit unsigned integer.  Length of the option, in octets,
     excluding the Option Type and Opt Data Len fields. */
    //uint8_t opt_data_len;

    /** The Identification field is set to a unique value and is copied
     into the Identification field of the Acknowledgement option
     when returned by the node receiving the packet over this hop. */
    uint16_t identification;

} dsr_ackreq_ext_t;

/******************************************************************************
 *
 * ACK - Acknowledgement
 *
 * "An Acknowledgement option MAY appear one or more times within a DSR Options
 *  header." RFC4728 p48
 *
 *  DONE handle multiple occurrences of ACK extension
 ******************************************************************************/

#define DSR_ACK_EXTENSION_HDRLEN 14

typedef struct __attribute__((__packed__)) dsr_ack_ext {

    /** 8-bit unsigned integer.  Length of the option, in octets,
     excluding the Option Type and Opt Data Len fields. */
    //uint8_t opt_data_len;

    /** Copied from the Identification field of the Acknowledgement
     Request option of the packet being acknowledged. */
    uint16_t identification;

    /** The address of the node originating the acknowledgement. */
    uint8_t ack_source_address[ETHER_ADDR_LEN];

    /** The address of the node to which the acknowledgement is to be
     delivered. */
    uint8_t ack_destination_address[ETHER_ADDR_LEN];

} dsr_ack_ext_t;

/******************************************************************************
 *
 * SOURCE - DSR SOURCE ROUTE
 *
 ******************************************************************************/

/** Length, in octets, of a dsr_source_ext excluding address */
#define DSR_SOURCE_EXTENSION_HDRLEN 4

/** Maximum count of addresses in a dsr_source_ext*/
#define DSR_SOURCE_MAX_ADDRESSES_IN_OPTION ((DESSERT_MAXEXTDATALEN - DSR_SOURCE_EXTENSION_HDRLEN) / ETHER_ADDR_LEN)

/** Initial opt_data_len size without any addresses */
#define DSR_SOURCE_INITIAL_OPT_DATA_LEN 3

#define dsr_source_get_address_count(source) ((source->opt_data_len - (3))/ ETHER_ADDR_LEN)

/** Returns the index to the next hop in the source extension indicated by
 * segments_left. Only valid, if
 *        source->segments_left > 0 !
 * Checking this before relying on the macro is a MUST. Otherwise you read
 * beyond end of struct! */
#define dsr_source_indicated_next_hop_index(source) ( dsr_source_get_address_count(source) - source->segments_left )

/** Returns the address of the first byte of the next hop address in the source
 * extension indicated by segments_left. Only valid, if
 *        source->segments_left > 0 !
 * Checking this before relying on the macro is a MUST. Otherwise you read
 * beyond end of struct! */
#define dsr_source_indicated_next_hop_begin(source) (source->address + (dsr_source_indicated_next_hop_index(source) * ETHER_ADDR_LEN))

/** Returns the index to the previous hop in the source extension indicated by
 * segments_left. */
#define dsr_source_previous_hop_index(source) (dsr_source_get_address_count(source) - source->segments_left -1)

/** Returns the address of the first byte of the previous hop address in the source
 * extension indicated by segments_left. */
#define dsr_source_previous_hop_begin(source) (source->address + (dsr_source_previous_hop_index(source) * ETHER_ADDR_LEN))

#define dsr_source_get_sizeof(source) (source->opt_data_len +1)

#define dsr_source_get_address_begin_by_index(source, i) (source->address + (i * ETHER_ADDR_LEN))

#define DSR_SOURCE_FIRST_LAST_HOP_EXTERNAL 128
#define DSR_SOURCE_FLAG_LAST_HOP_EXTERNAL   64

typedef struct __attribute__((__packed__)) dsr_source_ext {

    /** 8-bit unsigned integer.  Length of the option, in octets,
     excluding the Option Type and Opt Data Len fields.  For the
     format of the DSR Source Route option defined here, this field
     MUST be set to the value (n * 6) + 3, where n is the number of
     addresses present in the Address[i] fields. */
    uint8_t opt_data_len;

    /** Only the 2 (two) leftmost bits are significant
     (DSR_SOURCE_FIRST_LAST_HOP_EXTERNAL, DSR_SOURCE_FLAG_LAST_HOP_EXTERNAL).

     First Hop External (F)

     Set to indicate that the first hop indicated by the DSR Source
     Route option is actually an arbitrary path in a network
     external to the DSR network; the exact route outside the DSR
     network is not represented in the DSR Source Route option.
     Nodes caching this hop in their Route Cache MUST flag the
     cached hop with the External flag.  Such hops MUST NOT be
     returned in a Route Reply generated from this Route Cache
     entry, and selection of routes from the Route Cache to route a
     packet being sent SHOULD prefer routes that contain no hops
     flagged as External.

     Last Hop External (L)

     Set to indicate that the last hop indicated by the DSR Source
     Route option is actually an arbitrary path in a network
     external to the DSR network; the exact route outside the DSR
     network is not represented in the DSR Source Route option.
     Nodes caching this hop in their Route Cache MUST flag the
     cached hop with the External flag.  Such hops MUST NOT be
     returned in a Route Reply generated from this Route Cache
     entry, and selection of routes from the Route Cache to route a
     packet being sent SHOULD prefer routes that contain no hops
     flagged as External. */
    uint8_t flags;

    /** A 4-bit unsigned integer.  Count of number of times that this
     packet has been salvaged as a part of DSR routing (Section
     3.4.1). */
    uint8_t salvage;

    /** Number of route segments remaining, i.e., number of explicitly
     listed intermediate nodes still to be visited before reaching
     the final destination. */
    uint8_t segments_left;

    /** The sequence of addresses of the source route.  In routing and
     forwarding the packet, the source route is processed as
     described in Sections 8.1.3 and 8.1.5.  The number of addresses
     present in the Address[1..n] field is indicated by the Opt Data
     Len field in the option (n = (Opt Data Len - 3) / 6). */
    uint8_t address[DSR_SOURCE_MAX_ADDRESSES_IN_OPTION* ETHER_ADDR_LEN];

} dsr_source_ext_t;

#endif /* DSR_EXTENSIONS_H_ */
