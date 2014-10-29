/*!
*****************************************************************
* Dgps.cpp
*
* Copyright (c) 2013
* Fraunhofer Institute for Manufacturing Engineering
* and Automation (IPA)
*
*****************************************************************
*
* Repository name: seneka_sensor_node
*
* ROS package name: seneka_dgps
*
* Author: Ciby Mathew, E-Mail: Ciby.Mathew@ipa.fhg.de
* 
* Supervised by: Christophe Maufroy
*
* Date of creation: Jan 2013
* Modified 03/2014: David Bertram, E-Mail: davidbertram@gmx.de
* Modified 04/2014: Thorsten Kannacher, E-Mail: Thorsten.Andreas.Kannacher@ipa.fraunhofer.de
*
* Description: The seneka_dgps package is part of the seneka_sensor_node metapackage, developed for the SeNeKa project at Fraunhofer IPA.
* It implements a GNU/Linux driver for the Trimble BD982 GNSS Receiver Module as well as a ROS publisher node "DGPS", which acts as a wrapper for the driver.
* The ROS node "DGPS" publishes GPS data gathered by the DGPS device driver.
* This package might work with other hardware and can be used for other purposes, however the development has been specifically for this project and the deployed sensors.
*
*****************************************************************
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* - Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer. \n
* - Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution. \n
* - Neither the name of the Fraunhofer Institute for Manufacturing
* Engineering and Automation (IPA) nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission. \n
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License LGPL as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License LGPL for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License LGPL along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*
****************************************************************/

#include <seneka_dgps/Dgps.h>
#include <boost/optional.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/bind.hpp> 
#include <boost/asio.hpp>
#include <boost/thread.hpp>

void set_result1(boost::optional<boost::system::error_code>* a, boost::system::error_code b) 
{ 
	a->reset(b);
} 
void set_result2(boost::optional<boost::system::error_code>* a, boost::system::error_code b, size_t *bytes_transferred, size_t _bytes_transferred) 
{ 
	a->reset(b);
	*bytes_transferred = _bytes_transferred;
} 


template <typename MutableBufferSequence> 
int read_with_timeout(boost::asio::serial_port& sock, 
  const MutableBufferSequence& buffers) 
{ 
	boost::optional<boost::system::error_code> timer_result; 
	boost::asio::deadline_timer timer(sock.io_service()); 
	timer.expires_from_now(boost::posix_time::milliseconds(1000)); 
	timer.async_wait(boost::bind(set_result1, &timer_result, _1)); 


	size_t bytes_to_transfer = 0;
	boost::optional<boost::system::error_code> read_result; 
	boost::asio::async_read(sock, buffers, 
		boost::bind(set_result2, &read_result, _1, &bytes_to_transfer, boost::asio::placeholders::bytes_transferred)); 

	sock.io_service().reset(); 
	while (sock.io_service().run_one()) 
	{ 
	  if (read_result) 
		timer.cancel(); 
	  else if (timer_result) 
		sock.cancel(); 
	} 


	if (*read_result && !(*read_result==boost::asio::error::operation_aborted && bytes_to_transfer>0) ) {
		std::cout<<"error code: "<<(*read_result)<<std::endl;
	  return 0; //throw boost::system::system_error(*read_result);
	  }
	  
	return bytes_to_transfer;
} 

/*********************************************************/
/*************** Dgps class implementation ***************/
/*********************************************************/

// constructor
Dgps::Dgps() : m_SerialIO(io_service_) {
	boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service_));
}

// destructor
Dgps::~Dgps() {}

/**************************************************/
/**************************************************/
/**************************************************/

// takes diagnostic statements and stores them in diagnostic_array;
// if diagnostic_array holds more than 100 elements, the oldest stored element will get erased;
void Dgps::transmitStatement(DiagnosticFlag flag) {

    diagnostic_statement.diagnostic_message = msg.str();
    diagnostic_statement.diagnostic_flag    = flag;

    if (diagnostic_array.size() >= 100) {

        std::vector<DiagnosticStatement>::iterator it = diagnostic_array.begin();

        diagnostic_array.erase(it);
    
    }

    diagnostic_array.push_back(diagnostic_statement);

    // this expression clears the stringstream instance after each transmit process;
    // if it doesn't get cleared, every new diagnostic statement will get attached
    // to the existing ones within the object, so that it grows and grows...;
    msg.str("");

}

/**************************************************/
/**************************************************/
/**************************************************/

// establishes serial connection
bool Dgps::open(const char * pcPort, int iBaudRate) {

    //now comes a hack...
    char buf[256];
    sprintf(buf, "stty -F %s ispeed %d ospeed %d", pcPort, iBaudRate, iBaudRate);
    system(buf);

    msg << "Establishing serial connection to GPS device...";
    transmitStatement(INFO);

    msg << "Port: " << pcPort;
    transmitStatement(INFO);

    msg << "Baud rate: " << iBaudRate;
    transmitStatement(INFO);

    // open port;
    boost::system::error_code ec;
    m_SerialIO.open(pcPort, ec);

    if (ec) {
        msg << "Failed to establish connection. Device is not available on given port. "<<ec;
        transmitStatement(ERROR);

        return false;
    }

    else {
        msg << "Connection established.";
        transmitStatement(INFO);

        return true;
    }
    
    m_SerialIO.set_option(boost::asio::serial_port_base::baud_rate(iBaudRate), ec);
    if(ec) printf("failed apply settings (%s, %d)", ec.category().name(), (int)ec.value());
	m_SerialIO.set_option(boost::asio::serial_port_base::character_size(8), ec);
    if(ec) printf("failed apply settings (%s, %d)", ec.category().name(), (int)ec.value());
	m_SerialIO.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one), ec);
    if(ec) printf("failed apply settings (%s, %d)", ec.category().name(), (int)ec.value());
	m_SerialIO.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none), ec);
    if(ec) printf("failed apply settings (%s, %d)", ec.category().name(), (int)ec.value());
	m_SerialIO.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none), ec);
    if(ec) printf("failed apply settings (%s, %d)", ec.category().name(), (int)ec.value());
}

/**************************************************/
/**************************************************/
/**************************************************/

// checks the communication link by transmission of protocol request "ENQ" (05h);
// expected result is either "ACK" (06h) or "NAK" (05h); 
// see Trimble BD982 GNSS receiver manual, p. 65;
bool Dgps::checkConnection() {

    msg << "Testing the communication link...";
    transmitStatement(INFO);

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // test command "ENQ" (05h)
    char message[]      = {0x05};
    int message_size    = sizeof(message) / sizeof(message[0]);

    int bytes_sent  = 0;
    boost::system::error_code ec;
    bytes_sent      = m_SerialIO.write_some(boost::asio::buffer(message, message_size), ec); // function returns number of transmitted bytes;

    if (!(bytes_sent > 0)) {
        msg << "Could not send test command.";
        transmitStatement(ERROR);

        msg << "Communication link check failed. Device is not available.";
        transmitStatement(ERROR);
    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

	unsigned char result[1];
	const int num = read_with_timeout(m_SerialIO, boost::asio::buffer(result,sizeof(result)));
	
    /**************************************************/
    /**************************************************/
    /**************************************************/

    // analyzing received data;

    // checking if there is a response at all;
    if (!(num > 0)) {

        msg << "Device does not respond.";
        transmitStatement(ERROR);

        return false;
    }
    

    // checking for possible response "NAK" (15h);
    if (!(result[0] != 0x15)) {

        msg << "Test result is \"NAK\" (15h). Device is not ready yet.";
        transmitStatement(WARNING);

        return false;

    }

    // checking for expected result "ACK" (06h);
    if (!(result[0] != 0x06)) {

        msg << "Test result is \"ACK\" (06h) as expected.";
        transmitStatement(INFO);

        msg << "Testing the communications link succeeded. Device is available.";
        transmitStatement(INFO);

        return true;

    }

    // checking for unexpected reponse;
    else {

        msg << "Unknown test reponse packet. Device is not ready yet.";
        transmitStatement(WARNING);

        return false;

    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

}

/*******************************************************/
/*******************************************************/
/*******************************************************/

bool Dgps::getDgpsData() {

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // generation of request command packet "GETRAW" (56h);
    // expected reply packet is "RAWDATA" (57h); 
    // see Trimble BD982 GNSS Receiver Manual, p. 73/132;

    unsigned char stx           = 0x02; // head
    unsigned char status        = 0x00;
    unsigned char packet_type   = 0x56; // this command packet is of type "GETRAW" (56h);
    unsigned char length        = 0x03; // length of data part;
    unsigned char data_type     = 0x01; // raw data type:   position record --> type    = 00000001 (binary);
    unsigned char flags         = 0x01; // raw data format: concise         --> flags   = 00000001 (binary);
    unsigned char reserved      = 0x00; // tail
    unsigned char checksum;
    unsigned char etx           = 0x03;

    checksum = (status + packet_type + length + data_type + flags + reserved);

    char message[]      = {stx, status, packet_type, length, data_type, flags, reserved, checksum, etx};
    int message_size    = sizeof(message) / sizeof(message[0]);

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // transmission of request command
    int bytes_sent  = 0;
    boost::system::error_code ec;
    bytes_sent      = m_SerialIO.write_some(boost::asio::buffer(message, message_size), ec); // function returns number of transmitted bytes;

    if (bytes_sent != message_size) {
        msg << "Failed to transmit request command.";
        transmitStatement(WARNING);
    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // buffer for expected reply packet;
    // the size of the buffer is equal to the maximum size a "RAWDATA" (57h) reply packet of type 57h position record can get;
    // maximum_size = 8 bytes + 78 bytes + 2 * N bytes + 2 bytes = 112  bytes; (where N, the number of used sattelites, is 12);
    // minimum_size = 8 bytes + 78 bytes + 2 * N bytes + 2 bytes = 88   bytes; (where N, the number of used sattelites, is 0);
    // see Trimble BD982 GNSS Receiver Manual, p. 139;
    unsigned char buffer[112+16] = {0}; // why 112+16? --> see debugBuffer() function comment;
    
    // reading response from serial port;
	const int bytes_received = read_with_timeout(m_SerialIO, boost::asio::buffer(buffer,sizeof(buffer)));
	
    // see comment at debugBuffer() function;
    int bytes_received_debugged = bytes_received;// - 16;
    //std::vector<unsigned char> debugged_buffer = debugBuffer(buffer);
    unsigned char *debugged_buffer = buffer;

    /**************************************************/
    /**************************************************/
    /**************************************************/

    #ifndef NDEBUG

    // this debug output lists reply packet from receiver module after requesting a position record with "GETRAW" (56h) command;
    // there are three columns:
    // first column:    originally received data frame buffer[] from begin to end,
    // second column:   originally received data frame buffer[] omitting the first 16 bytes,
    // third column:    resulting debugged data frame debugged_buffer[] (must be the same as buffer[] in column 2);

    std::cout << endl << endl << "\t\t\t------------------------------" << endl << endl;

    std::cout << "bytes_received:\t\t" << bytes_received << endl;
    std::cout << "bytes_received_debugged:\t" << bytes_received_debugged << endl;


    int k = 16;

    for (int i = 0; i < (112+16); i++) {

        std::cout << "\nbuffer[" << i << "]:\t";
        printf("%x", buffer[i]);

        if (!(i >= 112)) {

            std::cout << "\tbuffer[" << k << "]:\t";
            printf("%x", buffer[k]);

            std::cout << "\tdebugged_buffer[" << i <<"]:\t";
            printf("%x", debugged_buffer[i]);

        }

        k++;
        
    }

    std::cout << endl << endl << "\t\t\t------------------------------" << endl << endl;

    #endif

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // raw analysis of received data;

    // checking if there has been a response at all;
    if (!(bytes_received_debugged > 0)) {

        msg << "Device does not respond.";
        transmitStatement(ERROR);

        return false;

    }

    // checking for possible "NAK" (15h) reply packet;
    else if (!(debugged_buffer[0] != 15)) {

        msg << "Response packet is \"NAK\" (15h). Device cannot fullfill request.";
        transmitStatement(WARNING);

        return false;

    }

    // checking for legal packet size;
    else if (!(88 >= bytes_received_debugged <= 112)) {

        msg << "Received packet has wrong size.";
        transmitStatement(WARNING);

        return false;

    }

    // checking for packet head (stx);
    else if (debugged_buffer[0] != 0x02) {

        msg << "First byte of received packet is not stx (0x02).";
        transmitStatement(WARNING);

        return false;

    }

    // checking for packet tail (etx)
    else if (debugged_buffer[bytes_received_debugged-1] != 0x03) {

        msg << "Last byte of received packet is not etx (0x03).";
        transmitStatement(WARNING);

        return false;

    }

    // checking for packet type;
    // must be "RAWDATA" (57h);
    else if (debugged_buffer[2] != 0x57) {

        msg << "Received packet has wrong type.";
        transmitStatement(WARNING);

        return false;

    }

    // checking for record type;
    // must be "Position Data" (01h);
    else if (debugged_buffer[4] != 0x01) {

        msg << "Received packet has wrong record type.";
        transmitStatement(WARNING);

        return false;

    }

    // checking pager counter;
    // must be 11h for position records;
    // 11 hex = 00010001 binary --> means page 01 of 01;
    else if (debugged_buffer[5] != 0x11) {

        msg << "Received packet has wrong page counter value.";
        transmitStatement(WARNING);

        return false;

    }

    // another raw checking may be applied here...;

    /**************************************************/
    /**************************************************/
    /**************************************************/

    else {

        // hereby called functions analyze the received packet in-depth, structure it, extract and finnaly serve GPS data;
        // if everything works fine, GPS data is getting stored in Dgps::GpsData gps_data;
        if (!analyzeData(debugged_buffer, bytes_received_debugged)) {

            msg << "Failed to gather GPS data.";
            transmitStatement(WARNING);

            return false;

        }

        else {

            return true;

        }

    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

}

/**************************************************/
/**************************************************/
/**************************************************/

bool Dgps::analyzeData(unsigned char *  buffer,         // points to received packet from serialIO (char array);
                       int              buffer_size) {  // number of received bytes;

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // --- header ---
    temp_packet.stx         = buffer[0];
    temp_packet.status      = buffer[1];
    temp_packet.packet_type = buffer[2];
    temp_packet.length      = buffer[3];

    // --- data part header ---
    temp_packet.record_type                 = buffer[4];
    temp_packet.page_counter                = buffer[5];
    temp_packet.reply_number                = buffer[6];
    temp_packet.record_interpretation_flags = buffer[7];

    // --- data part ---

    temp_packet.data_bytes.resize(temp_packet.length);

    for (int i = 0; i < (temp_packet.length - 4); i++)
        temp_packet.data_bytes[i] = buffer[8 + i];

    // --- tail ---
    temp_packet.checksum    = buffer[temp_packet.length + 4]       ;
    temp_packet.etx         = buffer[temp_packet.length + 4 + 1]   ;

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // verify checksum;
    unsigned char checksum = 0x00;
            
    // calculate checksum over data bytes;
    for (int i = 1; i < temp_packet.length+4; i++)
        checksum += buffer[i];

    // check for checksum mismatch;
    if (checksum != temp_packet.checksum) {

        msg << "Checksum mismatch. "<<(int)checksum<<" != "<<(int)temp_packet.checksum;
        transmitStatement(WARNING);

        return false;

    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

    else {

        return extractGpsData();

    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

}

/**************************************************/
/**************************************************/
/**************************************************/

bool Dgps::extractGpsData() {

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // get all fields of type DOUBLE (8 bytes);

    unsigned char latitude_bytes            [8];
    unsigned char longitude_bytes           [8];
    unsigned char altitude_bytes            [8];
    unsigned char clock_offset_bytes        [8];
    unsigned char frequency_offset_bytes    [8];
    unsigned char pdop_bytes                [8];
    unsigned char latitude_rate_bytes       [8];
    unsigned char longitude_rate_bytes      [8];
    unsigned char altitude_rate_bytes       [8];

    for (int i = 0; i < 8; i++) {

        latitude_bytes          [i] = temp_packet.data_bytes[i + 0];
        longitude_bytes         [i] = temp_packet.data_bytes[i + 8];
        altitude_bytes          [i] = temp_packet.data_bytes[i + 16];
        clock_offset_bytes      [i] = temp_packet.data_bytes[i + 24];
        frequency_offset_bytes  [i] = temp_packet.data_bytes[i + 32];
        pdop_bytes              [i] = temp_packet.data_bytes[i + 40];
        latitude_rate_bytes     [i] = temp_packet.data_bytes[i + 48];
        longitude_rate_bytes    [i] = temp_packet.data_bytes[i + 56];
        altitude_rate_bytes     [i] = temp_packet.data_bytes[i + 64];

    }

    // helper variable; used to find the meaning of semi-circles in this case...
    // (it's just 0-180 normalized to 0.0-1.0)
    double semi_circle_factor = 180.0;

    // getDOUBLE includes inverting of bit order;
    // neccessary because of motorola format;
    // see Trimbel BD982 GNSS Receiver Manual, p. 66ff;
    gps_data.latitude_value     = getDOUBLE(latitude_bytes) * semi_circle_factor;
    gps_data.longitude_value    = getDOUBLE(longitude_bytes) * semi_circle_factor;
    gps_data.altitude_value     = getDOUBLE(altitude_bytes);
    gps_data.clock_offset       = getDOUBLE(clock_offset_bytes);
    gps_data.frequency_offset   = getDOUBLE(frequency_offset_bytes);
    gps_data.pdop               = getDOUBLE(pdop_bytes);
    gps_data.latitude_rate      = getDOUBLE(latitude_rate_bytes);
    gps_data.longitude_rate     = getDOUBLE(longitude_rate_bytes);
    gps_data.altitude_rate      = getDOUBLE(altitude_rate_bytes);

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // get all fields of type LONG (4 bytes)

    unsigned char gps_msec_of_week_bytes[4];

    for (int i = 0; i < 4; i++) {

        gps_msec_of_week_bytes[i]   = temp_packet.data_bytes[i + 72];

    }

    // getLONG includes inverting of bit order
    gps_data.gps_msec_of_week = getLONG(gps_msec_of_week_bytes);

    /**************************************************/
    /**************************************************/
    /**************************************************/

    // get all fields of type CHAR (1 byte)

    gps_data.position_flags = getCHAR(temp_packet.data_bytes[76]);
    gps_data.number_of_SVs  = getCHAR(temp_packet.data_bytes[77]);

    // first getting rid of old values
    gps_data.channel_numbers.clear();
    gps_data.prn.clear();

    // gathering new values according to number of satellites
    for (int i = 0; i < gps_data.number_of_SVs; i++) {

        gps_data.channel_numbers.push_back  (getCHAR(temp_packet.data_bytes[78 + i*2]));
        gps_data.prn.push_back              (getCHAR(temp_packet.data_bytes[79 + i*2]));

    }

    /**************************************************/
    /**************************************************/
    /**************************************************/

    if (!(gps_data.number_of_SVs > 0)) {

        msg << "Currently no tracked sattelites.";
        transmitStatement(WARNING);

        return true; // return true, because this is a possible state and not an error!;

    }

    // some further checking may be added here...;
    // do not forget to return false in case of errors!;

    return true;

}

/**************************************************/
/**************************************************/
/**************************************************/

// for some reason, instead of just responding the requested "RAWDATA" (57h) position record packet, 
// the receiver module applys additional 16 bytes of data on top of the reply packet;
// these 16 bytes of data form another separate packet, including header, data part and tail;
// for now, I couldn't figure out why;
// to avoid this kind of mistake, the following workaround is necessary;
std::vector<unsigned char> Dgps::debugBuffer(unsigned char * buffer) {

    std::vector<unsigned char> debugged_buffer(112);

    int j = 16;

    // this assigns debugged_buffer[] with the byte values of the original received bytes in buffer[],
    // omitting the first 16 bytes;
    for (int i = 0; i < 112; i++) {
        debugged_buffer[i] = buffer[j];
        j++;
    }

    return debugged_buffer;

}

/**************************************************/
/**************************************************/
/**************************************************/

// function to reorder incoming bits
// neccessary because of motorola format;
// see Trimbel BD982 GNSS Receiver Manual, p. 66ff;
std::vector<bool> Dgps::invertBitOrder(bool * bits, Dgps::DataType data_type, bool invertBitsPerByte, bool invertByteOrder) {

	std::vector<bool> reversed_8_bit(8);
	std::vector<bool> reversed_16_bit(16);
	std::vector<bool> reversed_32_bit(32);
	std::vector<bool> reversed_64_bit(64);

    switch (data_type) {

        case CHAR:

            for (int i = 0; i < 8; i++) {

            if      (invertBitsPerByte)   reversed_8_bit[i] = bits[7-i];
            else if (!invertBitsPerByte)  reversed_8_bit[i] = bits[i  ];

            }

            return reversed_8_bit;

        case SHORT:

            for (int k = 0; k < 2; k++) {

                for (int i = 0; i < 8; i++) {

                if      (!invertByteOrder   && invertBitsPerByte)   reversed_16_bit[k * 8 + i] = bits[(k)      * 8 + (7 - i)   ];
                else if (!invertByteOrder   && !invertBitsPerByte)  reversed_16_bit[k * 8 + i] = bits[(k)      * 8 + (i)       ];
                else if (invertByteOrder    && invertBitsPerByte)   reversed_16_bit[k * 8 + i] = bits[(1 - k)  * 8 + (7 - i)   ];
                else if (invertByteOrder    && !invertBitsPerByte)  reversed_16_bit[k * 8 + i] = bits[(1 - k)  * 8 + (i)       ];
        
                }
    
            }

            return reversed_16_bit;

        case LONG:
        case FLOAT:
         
            for (int k = 0; k < 4; k++) {

                for (int i = 0; i < 8; i++) {

                if      (!invertByteOrder   && invertBitsPerByte)   reversed_32_bit[k * 8 + i] = bits[(k)      * 8 + (7 - i)   ];
                else if (!invertByteOrder   && !invertBitsPerByte)  reversed_32_bit[k * 8 + i] = bits[(k)      * 8 + (i)       ];
                else if (invertByteOrder    && invertBitsPerByte)   reversed_32_bit[k * 8 + i] = bits[(3 - k)  * 8 + (7 - i)   ];
                else if (invertByteOrder    && !invertBitsPerByte)  reversed_32_bit[k * 8 + i] = bits[(3 - k)  * 8 + (i)       ];
        
                }
    
            }

            return reversed_32_bit;

        case DOUBLE:

            for (int k = 0; k < 8; k++) {

                for (int i = 0; i < 8; i++) {

                if      (!invertByteOrder   && invertBitsPerByte)   reversed_64_bit[k * 8 + i] = bits[(k)      * 8 + (7 - i)   ];
                else if (!invertByteOrder   && !invertBitsPerByte)  reversed_64_bit[k * 8 + i] = bits[(k)      * 8 + (i)       ];
                else if (invertByteOrder    && invertBitsPerByte)   reversed_64_bit[k * 8 + i] = bits[(7 - k)  * 8 + (7 - i)   ];
                else if (invertByteOrder    && !invertBitsPerByte)  reversed_64_bit[k * 8 + i] = bits[(7 - k)  * 8 + (i)       ];
        
                }
    
            }

            return reversed_64_bit;

        default:

            return std::vector<bool>();

    }

}

/**************************************************/
/**************************************************/
/**************************************************/

char Dgps::getCHAR(unsigned char byte) {

    bool bits[8] = {0};

    for (int i = 0; i < 8; i++) {

        bits[i] = 0 != (byte & (1 << i));

    }

    std::vector<bool> resulting_bits = invertBitOrder(bits, CHAR);

    for (int i = 0; i < 8; i++) {

        bits[i] = resulting_bits[i];

    }

    char value = 0;

    for (int i = 0; i < 8; i++) {

        value = value + bits[i] * pow(2, 7 - i);

    }

    return value;

}

// function to extract numbers of data type LONG INTEGER from an 4-byte array;
// 8 bits per byte; array size is expected to be 4; ==> 32 bit;
long Dgps::getLONG(unsigned char * bytes) {

    bool bits[32] = {0};

    for (int k = 0; k < 4; k++) {

        for (int i = 0; i < 8; i++) {

            bits[((k * 4) + i)] = 0 != (bytes[k] & (1 << i));

        }

    }

    std::vector<bool> resulting_bits = invertBitOrder(bits, LONG);

    for (int i = 0; i < 32; i++) {

        bits[i] = resulting_bits[i];

    }

    long value = 0;

    for (int i = 0; i < 32; i++) {

        value = value + bits[i] * pow(2, 31 - i);

    }

    return value;

}

/**************************************************/
/**************************************************/
/**************************************************/

/* function to extract IEEE double precision number values from an 8-byte array
 * (8 bits per byte; array size is expected to be 8; ==> 64 Bit)
 *
 * bias is 1023 as default for standard numbers
 * ...(IEEE example -25.25 reads ok with invertedBitsPerByte and bias 1023)
 *
 * example data for -25.25:
 *
 * (8 bytes, last 5 bytes are 0x00, so written at initialization of array...)
 *
 *    unsigned char test_minus_25_25[8] = {0x00};
 *    test_minus_25_25[0]               = 0xc0;
 *    test_minus_25_25[1]               = 0x39;
 *    test_minus_25_25[2]               = 0x40;
 *
 * int exponent_bias = 1023;
 * hopefully working as default parameter... */
double Dgps::getDOUBLE(unsigned char * bytes, int exponent_bias) {

    // init with zero/false
    bool bits[64] = {false};

    // get bits of DOUBLE
    for (int k = 0; k < 8; k++) {

        for (int i = 0; i < 8; i++) {

            bits[((k * 8) + i)] = 0 != (bytes[k] & (1 << i));

        }

    }

    std::vector<bool> resulting_bits = invertBitOrder(bits, DOUBLE);
    for (int i = 0; i < 64; i++) bits[i] = resulting_bits[i];

    // calculate sign, fraction and exponent
    int sign_bit = bits[0];
    double exponent = 0;
    double fraction = 0;

    for (int i = 0; i < 11; i++) {

        exponent = exponent + bits[i + 1] * pow(2, 10 - i);

    }

    for (int i = 0; i < 52; i++) {

        fraction = fraction + bits[i + 12] * pow(0.5, i + 1);

    }

    // fraction is value between 1.0 and 2.0... see IEEE spec for details
    fraction += 1;

    int sign_lat = 1;
    if (sign_bit == 1) sign_lat = -1;

    // calculate number value from extracted values
    double double_value = sign_lat * (fraction * pow(2, exponent - exponent_bias));

    return double_value;

}

/**************************************************/
/**************************************************/
/**************************************************/
