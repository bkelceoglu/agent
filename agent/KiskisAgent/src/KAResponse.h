/**
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 *
 *    @copyright 2013 Safehaus.org
 */
/**
 *  @brief     KAResponse.h
 *  @class     KAResponse.h
 *  @details   KAResponse class is designed for marshaling and unmarshalling response messages.
 *  @author    Emin INAL
 *  @author    Bilal BAL
 *  @version   1.0.1
 *  @date      Dec 17, 2013
 */
#ifndef KARESPONSE_H_
#define KARESPONSE_H_

#include <syslog.h>
#include <iostream>
#include <jsoncpp/json.h>
#include <string>
#include <fstream>
using namespace std;
using std::stringstream;
using std::string;

class KAResponse
{
public:
	KAResponse( void );
	virtual ~KAResponse( void );
	string& getType();
	string& getUuid();
	int getRequestSequenceNumber();
	int getResponseSequenceNumber();
	int getExitCode();
	string& getStandardError();
	string& getStandardOutput();
	int getPid();
	string& getHostname();
	string& getMacAddress();
	string& getTaskUuid();
	int& 	getIsLxc();
	vector<string>& getIps();
	string& getSource();
	void setSource(const string&);
	void setHostname(const string&);
	void setMacAddress(const string&);
	void setTaskUuid(const string&);
	void setIsLxc(int);
	void setIps(vector<string>);
	void setPid(int);
	void setType(const string&);
	void setUuid(const string&);
	void setRequestSequenceNumber(int);
	void setResponseSequenceNumber(int);
	void setStandardError(const string&);
	void setStandardOutput(const string&);
	void setExitCode(int);
	void serialize(string&);						//Serializing a Chunk Response message to a Json String
	void serializeDone(string&);					//Serializing a Last Done Response message to a Json string
	void clear();
private:
	string        	type;
	string		    uuid;
	int			 	requestSequenceNumber;
	int			 	responseSequenceNumber;
	int				exitCode;
	int				pid;
	string       	stdOut;
	string        	stdErr;
	string			taskUuid;
	int				isLxc;
	string			macAddress;
	string			hostname;
	vector<string>  ips;
	string			source;
};
#endif /* KARESPONSE_H_ */
