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
 *  @brief     SubutaiContainerManager.h
 *  @class     SubutaiContainerManager.h
 *  @details   Manages containers on current host, uses LXC API.
 *  @author    Mikhail Savochkin
 *  @author    Ozlem Ceren Sahin
 *  @version   1.1.0
 *  @date      Oct 31, 2014
 */
#ifndef __SUBUTAI_CONTAINER_MANAGER_H__
#define __SUBUTAI_CONTAINER_MANAGER_H__

#include <string>
#include <vector>
#include <lxc/lxccontainer.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>

#include "SubutaiLogger.h"

using namespace std;

struct SubutaiContainer {
    string uuid;
    string hostname;
    lxc_container* container;
    vector<string> ip;
    vector<string> mac;
    map<int, string> users;
};

typedef std::vector<SubutaiContainer>::iterator ContainerIterator;

class SubutaiContainerManager {
    public:
        SubutaiContainerManager(string lxc_path, SubutaiLogger* logger);
        ~SubutaiContainerManager();
        void init();
        SubutaiContainer findContainer(string container_name);
        void findAllContainers();
        void findActiveContainers();
        void findDefinedContainers();
        bool isContainerRunning(string container_name);
        string RunProgram(SubutaiContainer* cont, string program, vector<string> params);
        void CollectInfo();
        void UpdateNetworkingInfo(SubutaiContainer* cont, string data);
        void UpdateUsersList(SubutaiContainer* cont);
    private:
        string                          _lxc_path;
        SubutaiLogger*                  _logger;
        lxc_container*                  _current_container;
        vector<SubutaiContainer>        _allContainers;
        vector<SubutaiContainer>        _definedContainers;
        vector<SubutaiContainer>        _activeContainers;
};

#endif
