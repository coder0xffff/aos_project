//============================================================================
// Name        : AOS_Project.cpp
// Author      : Ravi Nankani
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include "LamportClock.h"
#include "Cornet.h"
#include <cstdlib>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
using namespace std;

#define MAXDATASIZE 100 // max number of bytes we can get at once
#define PORT "3490"
#define BACKLOG 10
#define BUFMAX "50"
void* accept_connection(void *threadarg);
void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void* get_in_addr(struct sockaddr *sa) {
	if(sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((sockaddr_in*)sa)->sin_addr);
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while(std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

struct fileInput {
	unsigned int nodeid;
	unsigned int clockVal;
	string type;
	unsigned int param;
};

struct messagePayload {
	const char  *nodeid;
	char payLoad[50];
	int thread_id;
};

struct acceptConnectionData {
	string sender;
	intptr_t new_fd;
};

vector<string> nodes;
map<string,int> ip2node;
bool receivedIDLE = false;
bool initNode = false;
LamportClock lclock;
Cornet cornet;  // record which parents have sent a message
int D = 0;     // sum of deficits of outgoing edges
pthread_mutex_t clock_mutex;
pthread_mutex_t D_mutex;
pthread_mutex_t cornet_mutex;


void* send_message(void *threadarg);
void* send_master(void *threadarg);
char *currentNode;
int main(int argc, char **argv) {
	int sockfd;
	intptr_t new_fd;
	long t;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	pthread_t accept_thread;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	ifstream nodeFile;
	nodeFile.open("nodeList");
	string line;
	int i = 0;
	while(!nodeFile.eof()) {
		getline(nodeFile,line);
		nodes.push_back(line);
		ip2node[line] = i++;
	}

	currentNode = argv[1];

	pthread_mutex_init(&clock_mutex,NULL);
	pthread_mutex_init(&D_mutex,NULL);
	pthread_mutex_init(&cornet_mutex,NULL);
	pthread_t send_master_thread;
	pthread_create(&send_master_thread,NULL,send_master,(void*)t);
	if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		cerr<< "getaddrinfo " << gai_strerror(rv) <<endl;
		return 1;
	}

	for( p = servinfo; p != NULL ; p = p->ai_next) {
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1) {
			perror("server:socket");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) {
			perror("setsocketopt");
			exit(1);
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if(p == NULL) {
		cerr<<"server failed to bind"<<endl;
		return 2;
	}
	freeaddrinfo(servinfo);

	if(listen(sockfd,BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD,&sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	while(1) {
		sin_size = sizeof (their_addr);
		new_fd = accept(sockfd,(struct sockaddr *)&their_addr, &sin_size);
		if(new_fd == -1) {
			perror("accept");
			continue;
		}
		struct acceptConnectionData *acceptData = new struct acceptConnectionData;
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr),s,sizeof(s));
		acceptData->sender = s;
		acceptData->new_fd = new_fd;
		pthread_create(&accept_thread, NULL, accept_connection, (void *)acceptData);

	}
	return 0;
}


void* accept_connection(void *threadarg) {
	struct acceptConnectionData *acceptData = (struct acceptConnectionData *)threadarg;
	char buf[50];
	int numbytes;
	if((numbytes = recv(acceptData->new_fd,&buf,50,0)) == -1) {
		perror("receive");
		close(acceptData->new_fd);
		exit(0);
	}
	buf[numbytes] = '\0';
	vector<string> msgPayloadList;

	split(buf,' ',msgPayloadList);

	if(msgPayloadList.size() == 1) {
		// incoming is a message
		pthread_mutex_lock(&clock_mutex);
		int externTime = atoi(msgPayloadList[0].c_str()) + lclock.getClockRate();
		cout<< "internal time: " << lclock.getClockValue() << "external time: " << externTime << endl;
		lclock.tick(atoi(msgPayloadList[0].c_str()));

		cout << lclock.getClockValue() << "RECV "  << ip2node[acceptData->sender] <<endl;
		lclock.tick();
		pthread_mutex_unlock(&clock_mutex);
		pthread_mutex_lock(&cornet_mutex);
		cornet.addElement(acceptData->sender);
		cout << "C: before: " << cornet.size() -1 << " now: " << cornet.size() << endl;
		pthread_mutex_unlock(&cornet_mutex);
	}
	else {
		// incoming is a signal
		pthread_mutex_lock(&clock_mutex);
		int externTime = atoi(msgPayloadList[0].c_str()) + lclock.getClockRate();
		cout<< "internal time: " << lclock.getClockValue() << "external time: " << externTime << endl;
		lclock.tick(atoi(msgPayloadList[0].c_str()));
		cout << lclock.getClockValue() << " SIGNAL RECV: "  << ip2node[acceptData->sender] <<endl;
		lclock.tick();
		pthread_mutex_unlock(&clock_mutex);
		pthread_mutex_lock(&D_mutex);
		D--;
		cout<< "D: before: " << D+1 << " now: " << D << endl;
		pthread_mutex_unlock(&D_mutex);
	}
	close(acceptData->new_fd);
	pthread_exit(NULL);
}


/* decides to send or execute events from input file, along with deciding when to send signals to
 * processes in cornet
 */
void* send_master(void *threadarg) {
	ifstream iFile;
	string line;
	vector<string> paramData;
	list<fileInput>fileInputList;
	fileInput temp;
	struct messagePayload *msg;
	pthread_t send_thread;

	iFile.open("data");
	while(!iFile.eof()) {
		getline(iFile,line);
		split(line,' ',paramData);
		if(paramData[0] == currentNode) {
			temp.nodeid = atoi(paramData[0].c_str());
			temp.clockVal = atoi(paramData[1].c_str());
			temp.type = paramData[2];
			if(paramData.size() == 4)
				temp.param = atoi(paramData[3].c_str());
			fileInputList.push_back(temp);
		}
		paramData.clear();
	} // reading of input to do list finished at this point
	while(1) {
		if(!fileInputList.empty()) {
			struct fileInput currentAction;
			currentAction = fileInputList.front();
			pthread_mutex_lock(&clock_mutex);
			while(lclock.getClockValue() == currentAction.clockVal) {
				if(currentAction.type == "TICK") {
					cout<< lclock.getClockValue() << currentAction.type << currentAction.param <<endl;
					usleep(currentAction.param * 1000);
				}
				else if(currentAction.type == "IDLE") {
					cout<< lclock.getClockValue() << " " << currentAction.type <<endl;
					receivedIDLE = true;
				}
				else if(currentAction.type == "INIT") {
					cout<< lclock.getClockValue() << " "<< currentAction.type <<endl;
					initNode = true;
				}
				else if(currentAction.type == "SEND") {
					cout<< lclock.getClockValue() << " " << currentAction.type <<" "<< currentAction.param <<endl;
					msg = new struct messagePayload;
					msg->nodeid = nodes[currentAction.param].c_str();
					sprintf(msg->payLoad, "%d",lclock.getClockValue());
					pthread_mutex_lock(&D_mutex);
						D++;
					cout << "D before: " << D-1 <<" now: " << D << endl;
					pthread_mutex_unlock(&D_mutex);
					pthread_create(&send_thread,NULL,send_message,(void *)msg); // async thread call for sending message
				}
				lclock.tick();
				fileInputList.pop_front();
			}
		}

		pthread_mutex_unlock(&clock_mutex);
		pthread_mutex_lock(&cornet_mutex);
		if(!cornet.isEmpty()) {
			pthread_mutex_lock(&D_mutex);
			if( (receivedIDLE &&  D == 0 && cornet.size() == 1) || (cornet.size() > 1) ) {
				//send signal
				cout<<"C: " << cornet.size() << " D: " << D << endl;
				msg = new struct messagePayload;
				msg->nodeid = cornet.getElement().c_str();
				pthread_mutex_lock(&clock_mutex);
				sprintf(msg->payLoad, "%d SIGNAL",lclock.getClockValue());
				cout<< lclock.getClockValue() <<"SIGNAL SEND: " << ip2node[msg->nodeid] <<endl;
				cout<<"C: after sending: " << cornet.size() << endl;
				pthread_create(&send_thread,NULL,send_message,(void *)msg);
				lclock.tick();
				pthread_mutex_unlock(&clock_mutex);
			}
			pthread_mutex_unlock(&D_mutex);
		}
		else {
			//cornet is empty so we can try to check for termination

			if(initNode && fileInputList.empty() && D == 0) {
				cout << "terminated: " << lclock.getClockValue() << endl;
				break;
			}
		}
		pthread_mutex_unlock(&cornet_mutex);
	}

	pthread_exit(NULL);

}

void* send_message(void *threadarg) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct messagePayload *msg;
	int rv;
	char s[INET6_ADDRSTRLEN];
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	msg = ( struct messagePayload*) threadarg;
	if ((rv = getaddrinfo(msg->nodeid, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	// loop through all the results and connect to the first socketfd we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		//return 2;
	}
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	freeaddrinfo(servinfo); // all done with this structure
	if(send(sockfd,msg->payLoad,sizeof(msg->payLoad),0) == -1) {
		perror("send failed");
		exit(1);
	}

	close(sockfd);
	pthread_exit(NULL);
}
