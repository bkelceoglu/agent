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

#include "SubutaiContainerManager.h"

SubutaiContainerManager::SubutaiContainerManager(string lxc_path) : _lxc_path(lxc_path) 
{
}

SubutaiContainerManager::~SubutaiContainerManager() 
{
}

void SubutaiContainerManager::init() {
    // Check for running containers in case we just started an app
    // after crash
    char**              names;
    lxc_container**     cont;
    int num = list_active_containers(_lxc_path.c_str(), &names, &cont);
    for (int i = 0; i < num; i++) {
        SubutaiContainer c;
        c.uuid = "";
        c.id = 0;
        c.container = cont[i];
        _containers.push_back(c);
    }
}

bool SubutaiContainerManager::isContainerRunning(string container_name) 
{
    for (vector<SubutaiContainer>::iterator it = _containers.begin(); it != _containers.end(); it++) {
        if ((*it).uuid.compare(container_name) == 0) {
            return true;
        }
    }
    return false;
}

SubutaiContainer SubutaiContainerManager::findContainer(string container_name) {
    for (vector<SubutaiContainer>::iterator it = _containers.begin(); it != _containers.end(); it++) {
        if ((*it).uuid.compare(container_name) == 0) {
            return (*it);
        }
    }
}

bool SubutaiContainerManager::RunProgram(SubutaiContainer* cont, string program, vector<string> params) {
    char* _params[params.size() + 2];
    _params[0] = const_cast<char*>(program.c_str());
    vector<string>::iterator it;
    int i = 1;
    for (it = params.begin(); it != params.end(); it++, i++) {
        _params[i] = const_cast<char*>(it->c_str());
    }    
    _params[i + 1] = NULL;
    lxc_attach_options_t opts = LXC_ATTACH_OPTIONS_DEFAULT;
    int fd[2];
    pipe(fd);
    int _stdout = dup(1);
    dup2(fd[1], 1);
    char buffer[1000];
    cont->container->attach_run_wait(_current_container, &opts, program.c_str(), _params);
    fflush(stdout);
    string command_output;
    while (1) {
        ssize_t size = read(fd[0], buffer, 1000);
        command_output += buffer;
        if (size < 1000) {
            buffer[size] = '\0';
            command_output += buffer;
        }
    }
    dup2(_stdout, 1);
    return true;
}
