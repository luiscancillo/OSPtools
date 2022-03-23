/** @file GP2toOSP.cpp
 * Contains the command line program to generate an OSP file with SiRF IV receiver messages
 * from a GP2 debug file obtained from Android devices, like the smartphone Samsung Galaxy S2. 
 *<p>Usage:
 *<p>GP2toOSP.exe {options}
 *<p>Options are:
 *	- -D TODATE or --todate=TODATE : To date (dd/mm/aaaa). Default value TODATE = 31/12/2020
 *	- -d FROMDATE or --fromdate=FROMDATE : From date (dd/mm/aaaa). Default value FROMDATE = 01/01/2014
 *	- -i INFILE or --infile=INFILE : GP2 input file. Default value INFILE = SLCLog.GP2
 *	- -h or --help : Show usage data and stops. Default value HELP=FALSE
 *	- -l LOGLEVEL or --llevel=LOGLEVEL : Maximum level to log (SEVERE, WARNING, INFO, CONFIG, FINE, FINER, FINEST). Default value LOGLEVEL = INFO
 *	- -o OUTFILE or --outfile=OUTFILE : OSP binary output file. Default value OUTFILE = DATA.OSP
 *	- -T TOTIME or --totime=TOTIME : To time (hh:mm:sec). Default value TOTIME = 23:59:59
 *	- -t FROMTIME or --fromtime=FROMTIME : From time (hh:mm:sec). Default value FROMTIME = 00:00:00
 *	- -w WMSG or --wmsg=WMSG : Wanted messages MIDs (a comma separated list, ALL, RINEX,  or RINEX,list. Default value WMSG = RINEX
 *<p>
 *Copyright 2015, 2021 by Francisco Cancillo & Luis Cancillo
 *<p>
 *This file is part of the RXtoRINEX tool.
 *<p>
 *RXtoRINEX is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *RXtoRINEX is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *See the GNU General Public License for more details.
 *A copy of the GNU General Public License can be found at <http://www.gnu.org/licenses/>.
 *
 *Ver.	|Date	|Reason for change
 *------+-------+------------------
 *V1.0	|2/2015	|First release
 *V1.1	|2/2016	|Minor improvements for logging messages
 *V1.2	|2/2016	|Reviewed to run on Linux
 */

#include <string.h>
#include <time.h>

//from CommonClasses
#include "ArgParser.h"
#include "Logger.h"

using namespace std;

//@cond DUMMY
///The command line format
const string CMDLINE = "GP2toOSP.exe {options}";
///The current program version
const string MYVER = " V1.2";
//@cond DUMMY
///The parser object to store options and operators passed in the comman line
ArgParser parser;
//Metavariables for options
int INFILE, OUTFILE, HELP, LOGLEVEL, FROMDATE, TODATE, FROMTIME, TOTIME, WMSG;
//Metavariables for operators
//n/a
//Constraints used in this program
#define MSGSIZE 2050		//2048 (max payload size) + 2 (payload len)
#define WMSGSIZE 100		//the wanted message list maximum size
#define GP2SIZE 34 + MSGSIZE*3 + 12 + 1 + 1	//time tag chars + masg chars + (checksum + tail) + lf + null
#define START1 160	//0xA0	//OSP messages from/to receiver are preceded by the synchro
#define START2 162	//0xA2	//sequence of two bytes with values START1, START2
#define END1 176	//0XB0	//OSP messages from/to receiver are followed by the end
#define END2 179	//0XB3	//sequence of two bytes with values END1, END2
//variables and objects
char GP2line[GP2SIZE];			//a buffer for a line from the GP2 input file
unsigned char OSPmsg[MSGSIZE];	//a buffer to place the output binary OSP message
int OSPmsgSize = 0;				//current message size
//the list of OSP messages useful to obtain RINEX data
unsigned char WANTEDMsg[WMSGSIZE] = {2,6,7,56,8,11,12,15,28,50,64,75,0};
//prototypes of functions defined in this module
int extractMsgs(Logger*, FILE *, time_t, time_t, FILE *);
bool wantedMsg(unsigned char);
time_t dt2time (string);
bool checkInterval(string, time_t, time_t);
void addWANTED(string);
//@endcond 

/**main
 * gets the command line arguments, set parameters accordingly and translates data from the input GP2 file format to the output OSP file.
 * Input data are contained  in a SCLog.gp2 debug file generated by Android OS when the option DEBUGGING_FILES=1 is set in the sirfgps.conf file
 * of some devices, like the Samsung Galaxy S2 smartphone.
 *<p>
 * Each line in the GP2 file has a format as per the following example:
 *<p>
 * 29/10/2014 20:31:08.942 (0) A0 A2 00 12 33 06 00 00 00 00 00 00 00 19 00 00 00 00 00 00 64 E1 01 97 B0 B3
 *<p>
 * Where:
 * - Time tag: 29/10/2014 20:31:08.942
 * - Unknown: (0) 
 * - Head: A0 A2
 * - Payload length: 00 12
 * - Payload: 33 06 00 00 00 00 00 00 00 19 00 00 00 00 00 00 64 E1
 * - Checksum: 01 97
 * - Tail: B0 B3
 * Note that in above format, data from "head" to "tail" is an OSP messages with values written in hexadecimal.
 * A detailed definition of OSP messages can be found in the document "SiRFstarIV� One Socket Protocol Interface Control Document
 * Issue 9" from CSR Inc.
 *<p>
 * The binary OSP output files contain messages where head, check and tail have been removed, that is, the data for each
 * message consists of the two bytes of the payload length and the payload bytes.
 *<p>
 * Input data are processed according to the following criteria:
 * - Lines with incorrect message format are skipped. To be correct, a message shall start with A0A2, end with B0B3, its length shall match
 *   the number of bytes in the payload, and the computed payload checksum shall be equal to provided checksum. 
 * - Lines with time tag outside the time interval of validity are skipped. By default this interval is
 *   [01/01/2014 00:00:00 , 31/12/2020 23:59:59]. A different time interval can be defined using the related command options.
 * - Lines containing messages with MID not in the list of "wanted MIDs" are skipped.  By default this list includes the MID values 
 *   used to generate RINEX files (2,6,7,56,8,11,12,15,28,50,64,75). A different list can be defined using the related command options.
 *   Possibility exists to not filter messages (ALL MID wanted).
 *
 *@param argc the number of arguments passed from the command line
 *@param argv the array of arguments passed from the command line
 *@return the exit status according to the following values and meaning:
 *		- (0) no errors have been detected
 *		- (1) an error has been detected in arguments
 *		- (2) error when opening the input file
 *		- (3) error when creating output file
 */
int main(int argc, char* argv[]) {
	/**The main process sequence follows:*/
	/// 1- Defines and sets the error logger object
	Logger log("LogFile.txt", string(), string(argv[0]) + MYVER + string(" START"));
	/// 2- Setups the valid options in the command line. They will be used by the argument/option parser
	WMSG = parser.addOption("-w", "--wmsg", "WMSG", "Wanted mesages MIDs (a comma separated list, ALL, RINEX,  or RINEX,list", "RINEX");
	FROMTIME = parser.addOption("-t", "--fromtime", "FROMTIME", "From time (hh:mm:sec)", "00:00:00");
	TOTIME = parser.addOption("-T", "--totime", "TOTIME", "To time (hh:mm:sec)", "23:59:59");
	OUTFILE = parser.addOption("-o", "--outfile", "OUTFILE", "OSP binary output file", "DATA.OSP");
	LOGLEVEL = parser.addOption("-l", "--llevel", "LOGLEVEL", "Maximum level to log (SEVERE, WARNING, INFO, CONFIG, FINE, FINER, FINEST)", "INFO");
	HELP = parser.addOption("-h", "--help", "HELP", "Show usage data and stops", false);
	INFILE = parser.addOption("-i", "--infile", "INFILE", "GP2 input file", "SLCLog.GP2");
	FROMDATE = parser.addOption("-d", "--fromdate", "FROMDATE", "From date (dd/mm/aaaa)", "01/01/2014");
	TODATE = parser.addOption("-D", "--todate", "TODATE", "To date (dd/mm/aaaa)", "31/12/2020");
	/// 3- Parses arguments in the command line extracting options and operators
	try {
		parser.parseArgs(argc, argv);
	}  catch (string error) {
		parser.usage("Argument error: " + error, CMDLINE);
		log.severe(error);
		return 1;
	}
	log.info(parser.showOptValues());
	if (parser.getBoolOpt(HELP)) {
		//help info has been requested
		parser.usage("Generates an OSP file from a SP2 data file containing SiRF IV receiver messages", CMDLINE);
		return 0;
	}
	/// 4- Sets logging level stated in option
	log.setLevel(parser.getStrOpt(LOGLEVEL));
	/// 5- Sets the list of wanted messages
	string s = parser.getStrOpt (WMSG);
	if (s.compare("ALL") == 0) WANTEDMsg[0] = 0;
	else if (s.compare("RINEX") == 0) { }
	else if (s.find("RINEX,") == 0) addWANTED(s.substr(6));
	else addWANTED(s);
	//log list of message wanted
	s = string ("MID messages to OSP: ");
	if (WANTEDMsg[0] == 0) s += "ALL";
	else for (int i=0; i<WMSGSIZE && WANTEDMsg[i] != 0; i++) s += " " + to_string((long long) WANTEDMsg[i]);
	log.info(s);
	/// 6- Sets start and end time of the time interval for messages wanted 
	time_t startTime, endTime;
	startTime = dt2time(parser.getStrOpt (FROMDATE) + " " + parser.getStrOpt (FROMTIME));
	endTime = dt2time(parser.getStrOpt (TODATE) + " " + parser.getStrOpt (TOTIME));
	if ((startTime < 0) || (endTime < 0) || (startTime > endTime)) {
		log.severe("Incorrect From or To date or time option");
		return 1;
	}
	FILE *inFile;
	/// 7- Opens the SP2 input file
	if ((inFile = fopen(parser.getStrOpt(INFILE).c_str(), "r")) == NULL) {
		log.severe("Cannot open input file" + parser.getStrOpt(INFILE));
		return 2;
	}
	/// 8- Creates the OSP binary output file
	FILE *outFile;
	if ((outFile = fopen(parser.getStrOpt(OUTFILE).c_str(), "wb")) == NULL) {
		log.severe("Cannot create output file" + parser.getStrOpt(OUTFILE));
		return 3;
	}
	/// 9- Extracts/verifies/filters line by line messages from the SP2 file and translate/write them into OSP format
	int n = extractMsgs(&log, inFile, startTime, endTime, outFile);
	log.info("End of data extraction. Messages extracted: " + to_string((long long) n));
	fclose(inFile);
	fclose(outFile);
	return 0;
}
//@cond DUMMY
/**extractMsgs
 * extracts OSP messages contained in a SLCog.gp2 input file and writes them into a OSP binary output file.
 * Only messages having a time tag included in the time interval [fromT, toT] are extracted.
 * Only messages having a "wanted" MID are extracted.
 *
 * @param plog a pointer to the error logger
 * @param inFile the gp2 input file with GPS receiver messages
 * @param fromT defines the start of the time interval for messages to be extracted
 * @param toT defines the end of the time interval
 * @param outFile the output OSP file to place binary messages
 * @return the number of OSP messages extracted
 */
int extractMsgs(Logger* plog, FILE *inFile, time_t fromT, time_t toT, FILE *outFile) {
	char *header, *tail;
	unsigned int ui, payloadLen, computedCheck, messageCheck, nbytesRead;
	string timeTag;
	int nMessages = 0;

	//read input file line by line: each line shall be an OSP message
	while (fgets(GP2line, GP2SIZE, inFile) != NULL) {
		timeTag = string(GP2line, 23);
		//check if line time tag is in the wanted time interval
		if (!checkInterval(GP2line, fromT, toT)) {
			plog->finest(timeTag + " Time tag outside interval");
			continue;
		}
		header = strstr(GP2line, "A0 A2");	//find header
		tail =  strstr(header+5, "B0 B3");	//find tail
		if (header==NULL || tail==NULL) {	//log this error
			plog->warning(timeTag + " No message header or tailer");
			continue;
		}
		//get message data: length, payload, checksum
		header += 6;	//points now to the first mesage byte
		nbytesRead = 0;	//the counter for message bytes read
		while (header<tail && nbytesRead<MSGSIZE) {	//extract all message bytes
			if (sscanf(header, "%x", &ui)==1) {
				OSPmsg[nbytesRead] = ui;
				header += 3;
				nbytesRead++;
			}
			else header = tail;
		}
		//check message length
		if (nbytesRead>=MSGSIZE || nbytesRead<=4) {
			plog->warning(timeTag + " No message data");
			continue;
		}
		payloadLen = (OSPmsg[0] << 8) | OSPmsg[1];
		if (nbytesRead != payloadLen+4) {
			plog->warning(timeTag + " PayloadLen=" + to_string((long long) payloadLen) +
								"<>"  + to_string((long long) nbytesRead-4) + "=BytesRead" );
			continue;
		}
		//verify checksum
		computedCheck = 0;
		for (unsigned int i=0; i<payloadLen; i++) {
			computedCheck += OSPmsg[i+2];
			computedCheck &= 0x7FFF;
		}
		messageCheck = (OSPmsg[payloadLen+2] << 8) | OSPmsg[payloadLen+3];
		if (computedCheck != messageCheck) {
			plog->warning(timeTag + " Wrong checksum");
			continue;
		}
		//check if message MID is in the list of wanted ones
		if (wantedMsg(OSPmsg[2])) {
			//printf("wt|");
			//wanted, write it to the OSP output file
			if (fwrite(OSPmsg, 1, payloadLen+2, outFile) == payloadLen+2) nMessages++;
			else {
				plog->severe("Cannot writte to binary output file");
				return -nMessages - 4;
				break;
			}
			plog->fine(timeTag + " written MID " + to_string((long long) OSPmsg[2]));
		}
		else plog->finest(timeTag + " skipped MID " + to_string((long long) OSPmsg[2]));
	}
	return nMessages;
}

/**wantedMsg
 * checks if the given MID is in the list of "wanted" messages.
 *
 *@param mid is the Message IDentification code, as defined in the OSP ICD
 *@return true if current message is in the list, false otherwise
 **/
bool wantedMsg(unsigned char mid) {
	if (WANTEDMsg[0] == 0) return true;
	for (int i=0; i<WMSGSIZE && WANTEDMsg[i]!=0; i++)
		if (WANTEDMsg[i] == mid) return true;
	return false;
}

/**dt2time
 * converts date and time from the input line time tag string to a time_t value.
 *
 *@param dateAndTime a string having format dd/mm/yyyy hh:mm:ss
 *@return the given time as a time_t value, or -1 if date or time cannot be converted
 **/
time_t dt2time (string dateAndTime) {
	int a, b, c, d, e, f;
	struct tm *timeinfo;
	time_t rawtime;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	if (sscanf(dateAndTime.c_str(), "%d/%d/%d %d:%d:%d", &a, &b, &c, &d, &e, &f) == 6) {
		timeinfo->tm_mday = a;
		timeinfo->tm_mon = b - 1;
		timeinfo->tm_year = c - 1900;
		timeinfo->tm_hour = d;
		timeinfo->tm_min = e;
		timeinfo->tm_sec = f;
		return mktime(timeinfo);
	}
	return -1;	//wrong date or time
}
/**checkInterval
 * checks if the time tag provided is in the given time interval  [fromT, toT].
 *
 *@param timeTag the time tag string of the current line
 *@param fromT the initial time of the interval
 *@param toT the final time of the interval
 *@return true if the line time tag is in the time interval [fromT, toT], false otherwyse
 **/
 bool checkInterval(string timeTag, time_t fromT, time_t toT) {
	bool result = false;
	time_t tag = dt2time(timeTag);
	if (tag != -1) {
		result = (fromT <= tag) && (tag <= toT);
		}
	return result;
}

/**addWANTED
 * adds a list of comma separated MIDs to the "wanted" message list.
 *
 *@param midList is a comma separated list of MID to append to the list of wanted MIDs
 **/
 void addWANTED(string midList) {
	 char mids[301];
	 char *ptok;
	 int nextPos;

	 for (nextPos=0; nextPos<WMSGSIZE && WANTEDMsg[nextPos]!=0; nextPos++);
	 strncpy(mids, midList.c_str(), 300);
	 ptok = strtok(mids, ",;.:");
	 while (ptok != NULL) {
		WANTEDMsg[nextPos] = atoi(ptok);
		if (nextPos<WMSGSIZE && WANTEDMsg[nextPos]!=0) nextPos++;
		ptok = strtok(NULL, ",;.:");
	 }
 }
 //@endcond 
