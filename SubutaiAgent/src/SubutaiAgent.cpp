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
 *  @brief     SubutaiAgent.cpp
 *  @class     SubutaiAgent.cpp
 *  @details   This is SubutaiAgent Software main process.
 *  		   It's main responsibility is that send and receive messages from MQTT broker.
 *  		   It also creates a new process using KAThread Class when the new Execute Request comes.
 *  @author    Emin INAL
 *  @author    Bilal BAL
 *  @version   1.1.0
 *  @date      Sep 13, 2014
 */
/** \mainpage  Welcome to Project Subutai Agent
 *	\section   Subutai Agent
 * 			   The Subutai Agent is a simple daemon designed to connect securely to an MQTT server to
 * 			   reliably receive and send messages on queues and topics.
 * 	 	 	   It's purpose is to perform a very simple reduced set of instructions to
 * 	 	 	   manage any system administration task.
 * 	 	 	   The agent may run on physical servers, virtual machines or inside Linux Containers.
 */

#include "SubutaiCommand.h"
#include "SubutaiResponse.h"
#include "SubutaiUserID.h"
#include "SubutaiResponsePack.h"
#include "SubutaiThread.h"
#include "SubutaiConnection.h"
#include "SubutaiLogger.h"
#include "SubutaiWatch.h"
#include "SubutaiEnvironment.h"
#include "SubutaiContainerManager.h"
#include "SubutaiTimer.h"
#include <stdio.h>
#include <pthread.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <deque>

#define COMMAND_ID_MAX 300          // This is a maximum size of commandIdsList

using namespace std;

/**
 *  \details   threadSend function sends string messages in the Shared Memory buffer to MQTT Broker.
 *  	       This is a thread with working concurrently with main thread.
 *  	       It is main purpose that checking the Shared Memory Buffer in Blocking mode and sending them to Broker
 */
void threadSend(message_queue *mq, SubutaiConnection *connection,
		SubutaiLogger* logMain) {

	try {
		string str;
		unsigned int priority;
		size_t recvd_size;
		while (true) {
			str.resize(2500);
			mq->receive(&str[0], str.size(), recvd_size, priority);
			connection->sendMessage(str.c_str());
			logMain->writeLog(6,
					logMain->setLogData("<SubutaiAgent>::<threadsend>", str));
			str.clear();
		}
		message_queue::remove("message_queue");
	} catch (interprocess_exception &ex) {
		message_queue::remove("message_queue");
		std::cout << ex.what() << std::endl;
		logMain->writeLog(3,
				logMain->setLogData("<SubutaiAgent>::<threadsend>",
						"New exception Handled:", ex.what()));
	}
}

bool isCommandDuplicate(deque<string>* commandIdsList, string commandID,
		SubutaiLogger* logMain) {
	bool isCommandNew = true;

	for (deque<string>::iterator it = commandIdsList->begin();
			it != commandIdsList->end(); it++) {
		if ((*it) == commandID) {
			isCommandNew = false;
		}
	}

	if (!isCommandNew) {
		logMain->writeLog(6,
				logMain->setLogData("<SubutaiAgent>",
						"Command is not new!!!!!!!!!!!"));
		return true;
	} else {
		logMain->writeLog(6,
				logMain->setLogData("<SubutaiAgent>",
						"Command is new................."));
		commandIdsList->push_back(commandID);
		// Remove first command if amount of commands has exceeded maximum size
		if (commandIdsList->size() > COMMAND_ID_MAX) {
			commandIdsList->pop_front();
		}
		return false;
	}
}

/**
 *  \details   This function is the main thread of SubutaiAgent.
 *  		   It sends and receives messages from MQTT broker.
 *  		   It is also responsible from creation new process.
 */
int main(int argc, char *argv[], char *envp[]) {
	string serverAddress = "RESPONSE_TOPIC";           // Default RESPONSE TOPIC
	string broadcastAddress = "BROADCAST_TOPIC";	  // Default BROADCAST TOPIC
	string clientAddress;
	SubutaiHelper helper;
	SubutaiThread thread;
	SubutaiLogger logMain;
	SubutaiCommand* command = new SubutaiCommand();
	SubutaiResponsePack response;
	SubutaiEnvironment environment(&logMain);
	string input = "";
	string sendout;
	deque<string> commandIdsList;
	bool heartbeatInterruptFlag = false;
	bool waitHeartbeat = true; // helper for logging heartbeat waiting messages.

	if (!thread.getUserID().checkRootUser()) {
		//user is not root SubutaiAgent Will be closed
		cout
				<< "Main Process User is not root.. Subutai Agent is going to be closed.."
				<< endl;
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		return 100;
	}

	if (!logMain.openLogFileWithName("subutai-agent.log")) {
		cout << "/var/log/subutai-agent/ folder does not exist.."
				<< "Subutai Agent is going to be closed.." << endl;
		FILE* dumplog = fopen("/etc/subutai-agent_dump.log", "a+");
		string log = "<DEBUG> /var/log/subutai-agent/ folder does not exist.. "
				"Subutai Agent is going to be closed.. \n";
		fputs(log.c_str(), dumplog);
		fflush(dumplog);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		return 200;
	}

	bool hasLxc = false;
	try {
		if (system("which lxc-ls") != 0) {
			hasLxc = false;
		} else {
			hasLxc = true;
		}
	} catch (std::exception e) {
		logMain.writeLog(6,
				logMain.setLogData("<SubutaiAgent>", string(e.what())));
	}

	logMain.setLogLevel(7);
	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>",
					"Subutai Agent is starting.."));

	/*
	 * Getting Environment Settings..
	 */

	environment.getAgentSettings();
	environment.getAgentUuid();
	environment.getAgentInterfaces();
	environment.getAgentHostname();
	environment.getAgentEnvironmentId();
	environment.getAgentArch();
	clientAddress = environment.getAgentUuidValue();

	/*
	 * Starting Container Manager
	 */
	SubutaiContainerManager cman("/var/lib/lxc", &logMain);

	/*
	 * Opening MQTT Connection
	 */
	class SubutaiConnection *connection;
	int rc;
	mosqpp::lib_init();
	connection = new SubutaiConnection(clientAddress.c_str(),
			clientAddress.c_str(), serverAddress.c_str(),
			broadcastAddress.c_str(),
			environment.getAgentConnectionUrlValue().c_str(),
			atoi(environment.getAgentConnectionPortValue().c_str()),
			&environment, &logMain);

	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>",
					"Trying to open Connection with MQTT Broker: ",
					environment.getAgentConnectionUrlValue(), " Port: ",
					environment.getAgentConnectionPortValue()));

	int reconnectDelay = atoi(
			environment.getAgentConnectionOptionsValue().c_str()) - 4;

	if (reconnectDelay <= 0) {
		logMain.writeLog(3,
				logMain.setLogData("<SubutaiAgent>",
						"Reconnect Delay cannot be under 0",
						" Using default timeout(10).",
						helper.toString(reconnectDelay)));
		reconnectDelay = 10;
	}

	while (true) {
		if (!connection->openSession()) {
			sleep(reconnectDelay);
			logMain.writeLog(6,
					logMain.setLogData("<SubutaiAgent>",
							"Trying connect to MQTT Broker:",
							environment.getAgentConnectionUrlValue()));
			if (connection->reConnect()) {
				break;
			}
		} else {
			break;
		}
	}

	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>",
					"Connection Successfully opened with MQTT Broker: ",
					environment.getAgentConnectionUrlValue()));
	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>",
					"Registration Message is sending to MQTT Broker"));

	//logMain.writeLog(7,
	//		logMain.setLogData("<SubutaiAgent>",
	//				"Shared Memory MessageQueue is initializing.."));
	message_queue messageQueue(open_or_create,        //only create
			"message_queue",       //name
			100,                   //max message number
			2500                   //max message size
			);

	//logMain.writeLog(7,
	//		logMain.setLogData("<SubutaiAgent>",
	//				"Sending Thread is starting.."));
	boost::thread thread1(threadSend, &messageQueue, connection, &logMain);
	/* Change the file mode mask */
	umask(0);

	list<int> pidList;
	int ncores = -1;
	ncores = sysconf(_SC_NPROCESSORS_CONF);
	int currentProcess = 0;
	string str, str2;
	logMain.writeLog(7,
			logMain.setLogData("<SubutaiAgent>", "Number of cpu core:",
					helper.toString(ncores)));
	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>", "Client Address:",
					clientAddress));

	/*
	 * initializing inotify handler
	 */
	SubutaiWatch Watcher(connection, &response, &logMain);
	Watcher.initialize(20000);
	logMain.writeLog(7,
			logMain.setLogData("<SubutaiAgent>", "Watcher is initializing.."));

	/*
	 * initializing timer for periodical operations
	 */
	SubutaiTimer timer(logMain, &environment, &cman, connection);
	logMain.writeLog(7,
			logMain.setLogData("<SubutaiAgent>", "Timer is initializing.."));

	/*
	 * Send initial heartbeat for registration of resource host and
	 * container nodes attached to this host.
	 */
	timer.sendHeartBeat(false, false, &heartbeatInterruptFlag);
	logMain.writeLog(6,
			logMain.setLogData("<SubutaiAgent>", "Sending first heartbeat.."));
	while (true) {
		try {
			/*
			 * In 30 second periods send heartbeat and in_queue responses.
			 */

			timer.checkHeartBeatTimer(&heartbeatInterruptFlag);
			timer.checkCommandQueueInfoTimer();
			command->clear();
			for (list<int>::iterator iter = pidList.begin();
					iter != pidList.end(); iter++) {
				if (pidList.begin() != pidList.end()) {
					int status;
					pid_t result = waitpid(*iter, &status, WNOHANG);
					if (result != 0) {
						iter = pidList.erase(iter);
						currentProcess--;
						/*
						 string resp = response.createInQueueMessage(
						 environment.getAgentUuidValue(),
						 command->getCommandId());*/
					}
				}
			}

			Watcher.checkNotification(&cman); //checking the watch event status
			rc = connection->loop(1); // checking the status of the new message
			if (rc) {
				logMain.writeLog(6,
						logMain.setLogData("<SubutaiAgent>", "RC:",
								helper.toString(rc)));
				connection->reconnect();
			}

			//checking new message arrived
			if (connection->checkMessageStatus()) {
				command = connection->getMessage(); //fetching message..

				if (isCommandDuplicate(&commandIdsList,
						command->getCommandId(), &logMain)) {
					continue;
				}

				logMain.writeLog(6,
						logMain.setLogData("<SubutaiAgent>",
								"New Message is received:",
								command->getCommand()));

				// Check if this uuid belongs the resource host or one of child containers
				SubutaiContainer* target_container;
				if (hasLxc) {
					target_container = cman.findContainerById(
							command->getUuid());
				}

				if (hasLxc && target_container) {
					logMain.writeLog(7,
							logMain.setLogData("<SubutaiAgent>",
									"Container received a command to execute"));
				} else {
					logMain.writeLog(7,
							logMain.setLogData("<SubutaiAgent>",
									"Resource host received command to execute"));
				}

				if (command->getWatchArguments().size() != 0) {
					for (unsigned int i = 0;
							i < command->getWatchArguments().size(); i++) {
						logMain.writeLog(7,
								logMain.setLogData("<SubutaiAgent>",
										"Request WatchArgs:",
										command->getWatchArguments()[i]));
					}
				}

				if (cman.isManagerLocked()) {
					logMain.writeLog(7,
							logMain.setLogData("<SubutaiAgent>",
									"Execution is not permitted since container manager locked."));
				} else if (command->getType() == "PS_REQUEST") {
					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"PS execution operation is starting.."));
					SubutaiThread* subprocess = new SubutaiThread;
					subprocess->getLogger().setLogLevel(logMain.getLogLevel());
					if (!target_container) {
						command->setCommand(
								"for i in `ps aux | grep '[s]h -c' | awk -F \" \" '{print $2}'`;"
										" do ps aux | grep `pgrep -P $i` | sed '/grep/d' ;"
										" done 2> /dev/null");
						command->setWorkingDirectory("/");
					} else {
						string cmdline =
								"for i in `ps aux | grep '[s]h -c' | awk -F \" \" '{print $2}'`;"
										" do ps aux | grep `pgrep -P $i` | sed '/grep/d' ;"
										" done 2> /dev/null";
						target_container->PutToFile("/opt/psrun", cmdline);
						vector<string> chmod;
						chmod.push_back("+x");
						chmod.push_back("/opt/psrun");
						target_container->RunProgram("chmod", chmod);
						command->setCommand("/opt/psrun");
					}
					subprocess->threadFunction(&messageQueue, command, argv);

					delete subprocess;
				} else if (command->getType() == "TERMINATE_REQUEST") {
					int retstatus;
					string resp;
					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"Termination request is in progress."));

					if (command->getPid() > 0) {
						//logMain.writeLog(7, logMain.setLogData("<SubutaiAgent>","Killing given PID on resource host: " + command->getPid()));
						retstatus = kill(command->getPid(), SIGKILL);
						resp = response.createTerminateMessage(
								environment.getAgentUuidValue(),
								command->getCommandId(), command->getPid(),
								retstatus);

						connection->sendMessage(resp);
						logMain.writeLog(6,
								logMain.setLogData("<SubutaiAgent>",
										"Terminate response is ready. "));
						logMain.writeLog(7,
								logMain.setLogData("<SubutaiAgent>", resp));
					} else {
						logMain.writeLog(6,
								logMain.setLogData("<SubutaiAgent>",
										"Irrelevant Terminate Request"));
					}
				} else if (command->getType() == "SET_INOTIFY_REQUEST") {
					string preArg = "";
					if (target_container)
						preArg = "/var/lib/lxc/"
								+ target_container->getContainerHostnameValue()
								+ "/rootfs";

					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"Executing SET_INOTIFY_REQUEST.."));
					for (unsigned int i = 0;
							i < command->getWatchArguments().size(); i++) {
						Watcher.addWatcher(
								preArg + command->getWatchArguments()[i]);
						logMain.writeLog(7,
								logMain.setLogData("<SubutaiAgent>",
										"Adding Watcher: ",
										preArg
												+ command->getWatchArguments()[i]));
					}
					Watcher.stats();
					sendout = response.setInotifyResponse(
							environment.getAgentUuidValue(),
							command->getCommandId());
					connection->sendMessage(sendout, "RESPONSE_TOPIC");
					Watcher.stats();
				} else if (command->getType() == "UNSET_INOTIFY_REQUEST") {
					string preArg = "";
					if (target_container)
						preArg = "/var/lib/lxc/"
								+ target_container->getContainerHostnameValue()
								+ "/rootfs";

					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"Executing INOTIFY_CANCEL_REQUEST.."));
					for (unsigned int i = 0;
							i < command->getWatchArguments().size(); i++) {
						Watcher.eraseWatcher(
								preArg + command->getWatchArguments()[i]);
						logMain.writeLog(7,
								logMain.setLogData("<SubutaiAgent>",
										"Erasing Watcher: ",
										preArg
												+ command->getWatchArguments()[i]));
					}
					Watcher.stats();
					sendout = response.unsetInotifyResponse(
							environment.getAgentUuidValue(),
							command->getCommandId());
					connection->sendMessage(sendout, "RESPONSE_TOPIC");
					Watcher.stats();
				} else if (command->getType() == "LIST_INOTIFY_REQUEST") {
					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"Executing INOTIFY_SHOW_REQUEST.."));
					Watcher.stats();
					sendout = response.createInotifyShowMessage(
							environment.getAgentUuidValue(),
							command->getCommandId(), response.getConfPoints());
					connection->sendMessage(sendout, "RESPONSE_TOPIC");
					Watcher.stats();
				} else {
					logMain.writeLog(6,
							logMain.setLogData("<SubutaiAgent>",
									"Unknown command type.. "
											+ command->getType()));
				}

			} else {
				if (cman.isManagerLocked()
						&& connection->checkExecutionMessageStatus()) {
					logMain.writeLog(7,
							logMain.setLogData("<SubutaiAgent>",
									"Execution is not permitted since container manager locked."));
				} else if (heartbeatInterruptFlag) {
					if (waitHeartbeat) {
						logMain.writeLog(7,
								logMain.setLogData("<SubutaiAgent>",
										"Heartbeat is waiting to update list. Don't run any more commands until it sends heartbeat."));
						waitHeartbeat = false;
					}
				} else if (currentProcess < ncores
						&& connection->checkExecutionMessageStatus()) {
					waitHeartbeat = true;
					command = connection->getExecutionMessage();

					if (isCommandDuplicate(&commandIdsList,
							command->getCommandId(), &logMain)) {
						continue;
					}

					logMain.writeLog(7,
							logMain.setLogData("<SubutaiAgent>",
									"Execute operation is starting for "
											+ command->getCommandId()));
					SubutaiThread* subprocess = new SubutaiThread;
					subprocess->getLogger().setLogLevel(logMain.getLogLevel());
					SubutaiContainer* target_container = cman.findContainerById(
							command->getUuid());
					pidList.push_back(
							subprocess->threadFunction(&messageQueue, command,
									argv, target_container));
					currentProcess++;
					delete subprocess;
				}
			}
		} catch (const std::exception& error) {
			logMain.writeLog(6,
					logMain.setLogData("<SubutaiAgent>",
							"Exception is raised: ", error.what()));
			cout << error.what() << endl;
		}
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	logMain.writeLog(7,
			logMain.setLogData("<SubutaiAgent>",
					"Subutai Agent is closing Successfully.."));
	logMain.closeLogFile();
	kill(getpid(), SIGKILL);
	mosqpp::lib_cleanup();
	return 0;
}
