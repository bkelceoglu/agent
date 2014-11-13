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
 *    @copyright 2014 Safehaus.org
 */
/**
 *  @brief     SubutaiContainer.cpp
 *  @class     SubutaiContainer.cpp
 *  @details   SubutaiContainer Class is designed for getting and setting container variables and special informations.
 *  		   This class's instance can get get useful container specific informations
 *  		   such as IPs, UUID, hostname, macID, parentHostname, etc..
 *  @author    Mikhail Savochkin
 *  @author    Ozlem Ceren Sahin
 *  @version   1.1.0
 *  @date      Oct 31, 2014
 */
#include "SubutaiContainer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>

using namespace std;
/**
 *  \details   Default constructor of SubutaiContainer class.
 */
SubutaiContainer::SubutaiContainer(SubutaiLogger* logger, lxc_container* cont)
{
    this->container = cont;
    this->containerLogger = logger;
}

/**
 *  \details   Default destructor of SubutaiContainer class.
 */
SubutaiContainer::~SubutaiContainer()
{
    // TODO Auto-generated destructor stub
}

/**
 *  \details   Clear id, mac address and ip adresses.
 */
void SubutaiContainer::clear()
{
    id = "";
    interfaces.clear();
}

/**
 * Run program given as parameter 'program' with arguments 'params'
 * Return stdout if success or stderr if fails
 */
string SubutaiContainer::RunProgram(string program, vector<string> params) 
{
    ExecutionResult result = RunProgram(program, params, true, LXC_ATTACH_OPTIONS_DEFAULT);
    if (result.exit_code == 0) {
        return result.out;
    } else {
        return result.err;
    }
}

/**
 * Run program given as parameter 'program' with arguments 'params' using lxc attach options 'opts'
 * Returns ExecutionResult object including exit_code and stdout if success or stderr if fails.
 *
 */
ExecutionResult SubutaiContainer::RunProgram(string program, vector<string> params, bool return_result, lxc_attach_options_t opts, bool captureOutput) 
{
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Running program: ", program));

    // get arguments list of the command which will be run on lxc

    char* _params[params.size() + 2];
    _params[0] = const_cast<char*>(program.c_str());
    vector<string>::iterator it;
    int i = 1;
    for (it = params.begin(); it != params.end(); it++) {
		_params[i] = const_cast<char*>((*it).c_str());
		i++;
	}
    _params[i] = NULL;

#if _DEBUG
    for (int __j = 0; __j < params.size() + 2; __j++) {
        cout << "<DEBUG> PARAMS DATA: " << _params[__j] << endl;
    }
#endif

    // run command on LXC and read stdout into buffer.
    int fd[2];
    int _stdout = dup(1);
    ExecutionResult result;
    char buffer[1000];

    if (captureOutput) {
        pipe(fd);
        dup2(fd[1], 1);
        fflush(stdout);
        fflush(stderr);
        close(fd[1]);
    }
    result.exit_code = this->container->attach_run_wait(this->container, &opts, program.c_str(), _params);
    if (captureOutput) {
        fflush(stdout);
        close(fd[1]);
        dup2(_stdout, 1);
        close(_stdout);
        string command_output;
        while (1) {
            ssize_t size = read(fd[0], buffer, 1000);
            if (size < 1000) {
                buffer[size] = '\0';
                command_output += buffer;
                break;
            } else {
                command_output += buffer;
            }
        }


        //   get exit code, stdout and stderr.

        if (result.exit_code == 0) {
            result.out = command_output;
        } else {
            result.err = command_output;
        }
    }
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>",program + "executed. Exit code: "+ _helper.toString(result.exit_code)
    		+ ", out stream: " + result.out + ", err stream: " + result.err));
    return result;
}


/**
 *  \details   get the users defined on LXC
 */
void SubutaiContainer::UpdateUsersList() 
{ 
    if(status != RUNNING) return ;
    this->_users.clear();
    vector<string> params;
    params.push_back("/etc/passwd");
    string passwd = RunProgram("/bin/cat", params);
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Get users defined on LXC"));

    stringstream ss(passwd);
    string line;
    while (getline(ss, line, '\n')) {
        int uid;
        string uname;
        std::size_t found_first  = line.find(":");
        std::size_t found_second = line.find(":", found_first+1);
        std::size_t found_third  = line.find(":", found_second+1);
        uname = line.substr(0, found_first);
        uid   = atoi(line.substr(found_second+1, found_third).c_str());
        this->_users.insert(make_pair(uid, uname));
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Adding user: " + _helper.toString(uid) + " " + uname));
    }
}
/**
 *  \details   ID of the Subutai Container is fetched from statically using this function.
 *  		   Example id:"ff28d7c7-54b4-4291-b246-faf3dd493544"
 */
bool SubutaiContainer::getContainerId()
{
    try
    {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Get container id"));
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Check uuid.txt.."));
    	string path = "/var/lib/lxc/" + this->hostname + "/rootfs/etc/subutai-agent/";
        string uuidFile = path + "uuid.txt";
        ifstream file(uuidFile.c_str());	//opening uuid.txt
        getline(file,this->id);
        file.close();

        if (this->id.empty())		//if uuid is null or not reading successfully
        {
            containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "uuid.txt is empty. Generate uuid.."));
            boost::uuids::random_generator gen;
            boost::uuids::uuid u = gen();
            const std::string tmp = boost::lexical_cast<std::string>(u);
            this->id = tmp;

            struct stat sb;
            if (!(stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)))
            {
                containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "create directory: subutai-agent.."));
            	int status = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH);
            	if(status != 0) containerLogger->writeLog(1,containerLogger->setLogData("<SubutaiContainer>","\"subutai-agent\" folder cannot be created under /etc/"));
            }

            containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "write generated uuid into uuid.txt.."));
            ofstream file(uuidFile.c_str());
            file << this->id;
            file.close();

            containerLogger->writeLog(1,containerLogger->setLogData("<SubutaiContainer>","Subutai Agent UUID: ",this->id));
            return false;
        }
        return true;
    } catch(const std::exception& error) {
        cout << error.what()<< endl;
    }
    return false;
}

/**
 *  \details   get mac ids of the Subutai Container is fetched from statically.
 */
bool SubutaiContainer::getContainerInterfaces()
{
    interfaces.clear();
    if (this->status != RUNNING) return false;

    vector<string> v;
    string result = RunProgram("ifconfig", v);
    vector<string> lines = _helper.splitResult(result, "\n");
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Run ifconfig on LXC"));
    string nic = "", address = "", ip = ""; bool found_name=false, found_mac = false, found_ip = false;
    for (vector<string>::iterator it = lines.begin(); it != lines.end(); it++)
    {
    	vector<string> splitted = _helper.splitResult((*it), " ");
    	if((*it).at(0) != ' ')
    	{
    		found_name = true; found_mac = false; found_ip = false;
    		nic = splitted[0];
    	}
    	if(splitted.size() > 0)
    	{
			bool found_m = false, found_i = false;

			for (vector<string>::iterator it_s = splitted.begin(); it_s != splitted.end(); it_s++)
			{
				if(found_m)
				{
					found_mac = true;
					address = *it_s;
					found_m = false;
				}
				if(!strcmp((*it_s).c_str(), "HWaddr")) found_m = true;
				if(found_i)
				{
					found_ip = true;
					ip = _helper.splitResult((*it_s), " ")[1];
					found_i = false;
				}
				if(!strcmp((*it_s).c_str(), "inet")) found_i = true;
			}

			if(found_mac && found_name && found_ip)
			{
				struct Interface interface_n;
				interface_n.name = nic; interface_n.mac = address; interface_n.ip = ip;
				interfaces.push_back(interface_n);
			    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Adding interface: " + nic + " " + address + " " + ip));
			    found_mac = false; found_ip = false; found_name = false; nic = ""; address = ""; ip = "";
			}
    	}
    }

    return true;
}


/**
 *  \details   set the hostname of Subutai Container.
 */
void SubutaiContainer::setContainerHostname(string hostname)
{
    this->hostname = hostname;
}

/**
 *  \details   get the status of Subutai Container.
 */
string SubutaiContainer::getContainerStatus()
{
    if (this->status == RUNNING) return "RUNNING";
    if (this->status == STOPPED) return "STOPPED";
    if (this->status == FROZEN)  return "FROZEN";
    return "ERROR";
}

/**
 *  \details   set the status of Subutai Container.
 */
void SubutaiContainer::setContainerStatus(containerStatus status)
{
    this->status = status;
}


/**
 *  \details   IpAddress of the SubutaiContainer machine is fetched from statically.
 *//*
bool SubutaiContainer::getContainerIpAddress()
{
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "get container ip addresses"));

    if (this->status != RUNNING) return false;
    ipAddress.clear();
    char** interfaces = this->container->get_interfaces(this->container);
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "get container interfaces"));

    int i = 0;
    if (interfaces != NULL)
    {
        while (interfaces[i] != NULL) {
            char** ips = this->container->get_ips(this->container, interfaces[i], "inet", 0);
            containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "get container ips for current interface"));
            int j = 0;
            while (ips[j] != NULL) {
                ipAddress.push_back(ips[j]);
                containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Adding " + ipAddress.at(ipAddress.size() - 1) + ".."));
                j++;
            }
            i++;
        }
    }
    free(interfaces);
    if (ipAddress.size() > 0) {
        return true;
    } else {
        return false;
    }
}*/

void SubutaiContainer::write()
{
    cout << hostname << " " << id << endl;
}

/**
 *  \details   getting SubutaiContainer uuid value.
 */
string SubutaiContainer::getContainerIdValue()
{
    return id;
}

/**
 *  \details   getting SubutaiContainer hostname value.
 */
string SubutaiContainer::getContainerHostnameValue()
{
    return hostname;
}

/**
 *  \details   getting SubutaiContainer lxc container value.
 */
lxc_container* SubutaiContainer::getLxcContainerValue()
{
    return container;
}

/**
 *  \details   getting SubutaiContainer macaddress value for a given interface.
 */
vector<Interface> SubutaiContainer::getContainerInterfaceValues()
{
	return interfaces;
}



/**
 *  \details   update all field of Subutai Container
 */
void SubutaiContainer::getContainerAllFields()
{
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "get all container fields"));
    clear();
    getContainerId();
    getContainerInterfaces();

    UpdateUsersList();
}

ExecutionResult SubutaiContainer::RunCommand(SubutaiCommand* command) 
{
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Running command.. "));
	// set default lxc attach options
    lxc_attach_options_t opts = LXC_ATTACH_OPTIONS_DEFAULT;

    // set working directory for lxc_attach
    if (command->getWorkingDirectory() != "" && checkCWD(command->getWorkingDirectory())) {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "change working directory.."));
        opts.initial_cwd = const_cast<char*>(command->getWorkingDirectory().c_str());
    }

    // set run as parameter
    if (command->getRunAs() != "" && checkUser(command->getRunAs())) {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "change user running command by.."));
        opts.uid = getRunAsUserId(command->getRunAs());
    }

    // Settings env variables
    list< pair<string, string> >::iterator it;
    int i = 0;
    for (it = command->getEnvironment().begin(); it != command->getEnvironment().end(); it++, i++) {
        stringstream ss;
        ss << it->first << "=" << it->second;
        strcpy(opts.extra_env_vars[i], ss.str().c_str());
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "set environment " + ss.str()));
    }

    // divide program and arguments if all arguments are given in program field of command
    vector<string> pr = ExplodeCommandArguments(command);
    string program = pr[0];
    vector<string> args;
    for (vector<string>::iterator it = pr.begin()+1; it != pr.end(); it++) {
        args.push_back((*it));
    }

    // execute program on LXC
    ExecutionResult res = RunProgram(program, args, true, opts, false);

    return res;
}

ExecutionResult SubutaiContainer::RunDaemon(SubutaiCommand* command) 
{
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Running daemon. "));
	// set default lxc attach options
    lxc_attach_options_t opts = LXC_ATTACH_OPTIONS_DEFAULT;

    // set working directory for lxc_attach
    if (command->getWorkingDirectory() != "" && checkCWD(command->getWorkingDirectory())) {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "change working directory.."));
        opts.initial_cwd = const_cast<char*>(command->getWorkingDirectory().c_str());
    }

    // set run as parameter
    if (command->getRunAs() != "" && checkUser(command->getRunAs())) {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "change user running command by.."));
        opts.uid = getRunAsUserId(command->getRunAs());
    }

    // Settings env variables
    list< pair<string, string> >::iterator it;
    int i = 0;
    for (it = command->getEnvironment().begin(); it != command->getEnvironment().end(); it++, i++) {
        stringstream ss;
        ss << it->first << "=" << it->second;
        strcpy(opts.extra_env_vars[i], ss.str().c_str());
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "set environment " + ss.str()));
    }

    // divide program and arguments if all arguments are given in program field of command
    vector<string> pr = ExplodeCommandArguments(command);
    string program = "subutai-run";
    vector<string> args;
    for (vector<string>::iterator it = pr.begin(); it != pr.end(); it++) {
        args.push_back((*it));
    }
    args.push_back(command->getCommandId());

    // execute program on LXC
    ExecutionResult res = RunProgram(program, args, true, opts, false);
    return res;
}

// We need to check if CWD is exist because in LXC API - if cwd does not
// exist CWD will become root directory
bool SubutaiContainer::checkCWD(string cwd) 
{
    vector<string> params;
    params.push_back(cwd);
    ExecutionResult result = RunProgram("ls", params, true, LXC_ATTACH_OPTIONS_DEFAULT);    
    if (result.exit_code == 0) { 
        return true;
    } else {
        containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "working directory not found: "+ cwd + "on " + this->hostname));
        return false;
    }
}

/*
 * /details     Runs throught the list of userid:username pairs
 *              and check user existence
 */
bool SubutaiContainer::checkUser(string username) 
{
    if (_users.empty()) {
        UpdateUsersList();
    }
    for (user_it it = _users.begin(); it != _users.end(); it++) {
        if ((*it).second.compare(username) == 0) {
            return true;
        }
    } 
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "user not found: "+ username + "on " + this->hostname));
    return false;
}


/*
 * /details     Runs through the list of userid:username pairs
 *              and returns user id if username was found
 */
int SubutaiContainer::getRunAsUserId(string username) 
{
    if (_users.empty()) {
        UpdateUsersList();
    }
    for (user_it it = _users.begin(); it != _users.end(); it++) {
        if ((*it).second.compare(username) == 0) {
            return (*it).first;
        }
    } 
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "user not found: "+ username + "on " + this->hostname));
    return -1;
}

/**
 * \details		Write info into a file on LXC
 */
void SubutaiContainer::PutToFile(string filename, string text) {
    vector<string> args;
    args.push_back("-c");
    args.push_back("'/bin/echo");
    args.push_back(text);
    args.push_back(">");
    args.push_back(filename);
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Echo "+ text + "in " + filename));
    RunProgram("/bin/bash", args);
}

/**
 * \details		Get the full path for a given program
 */
string SubutaiContainer::findFullProgramPath(string program_name) 
{
    vector<string> args;
    args.push_back(program_name);
    string locations = RunProgram("whereis", args);
    return _helper.splitResult(program_name, "\n")[1];
}

/**
 * \details 	run ps command on LXC.
 */
string SubutaiContainer::RunPsCommand() {
    vector<string> args;
    containerLogger->writeLog(1, containerLogger->setLogData("<SubutaiContainer>", "Running ps command.."));
    return RunProgram("/opt/psrun", args);
}

/**
 * \details 	check and divide command and arguments if necessary.
 */
vector<string> SubutaiContainer::ExplodeCommandArguments(SubutaiCommand* command) 
{
    vector<string> result;
    size_t p = 0;
    size_t n = 0;
    while ((n = command->getCommand().find_first_of(" ", p)) != string::npos) {
        if (n - p != 0) {
            result.push_back(command->getCommand().substr(p, n - p));
        }
        p = n + 1;
    } 
    if (p < command->getCommand().size()) {
        result.push_back(command->getCommand().substr(p));
    }
    for(unsigned int i = 0; i < command->getArguments().size(); i++)
        result.push_back(command->getArguments()[i]);

    return result;
}

/**
 * For testing purpose
 *
 * Test if long commands with && can run or not:
 * It waits until all the commands run to return.
 */
void SubutaiContainer::tryLongCommand() {
    vector<string> args;
    args.push_back("-c");
    args.push_back("ls -la && ls && ls -la && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls && ls && sleep 2 && ls && ls -la && ls && ls -la && ls");
    cout << RunProgram("/bin/bash", args) << endl;
}

