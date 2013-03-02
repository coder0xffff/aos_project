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
template <typename T, size_t N>
T* begin(T(&arr)[N]) {
	return &arr[0];
}

template <typename T, size_t N>
T* end (T(&arr)[N]) {
	return &arr[N];
}
string nodes[] = {"192.168.1.14","192.168.1.15"};
//vector<string> nodeList(begin(nodes),end(nodes));

LamportClock lclock;
pthread_mutex_t clock_mutex;
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
	currentNode = argv[1];
	pthread_mutex_init(&clock_mutex,NULL);
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

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr),s,sizeof(s));
		cout<<"got connection from" << s << endl;
		pthread_create(&accept_thread, NULL, accept_connection, (void *)new_fd);

	}
	return 0;
}


void* accept_connection(void *threadarg) {
	//close(sockfd);
	int new_fd = (intptr_t) threadarg;
	char buf[50];
	int numbytes;
	if((numbytes = recv(new_fd,&buf,50,0)) == -1) {
		perror("receive");
		close(new_fd);
		exit(0);
	}
	buf[numbytes] = '\0';
	//cout<<"server received " << buf <<endl; // value of clock at senders side
	pthread_mutex_lock(&clock_mutex);
	lclock.tick(atoi(buf));
	cout << lclock.getClockValue() << "RECV"  <<endl;
	lclock.tick();
	pthread_mutex_unlock(&clock_mutex);
	close(new_fd);
	pthread_exit(NULL);
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

void* send_master(void *threadarg) {
	ifstream iFile;
	string line;
	vector<string> paramData;
	list<fileInput>fileInputList;
	fileInput temp;
	struct messagePayload *msg = new struct messagePayload;
	pthread_t send_thread;

	iFile.open("data");
	while(!iFile.eof()) {
		getline(iFile,line);
		split(line,' ',paramData);
		//cout<<" line no " << ++lineNo <<endl;
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
	while(!fileInputList.empty()) {
		struct fileInput currentAction;
		currentAction = fileInputList.front();
		// check later and see if we can read value without doing a mutex check
		pthread_mutex_lock(&clock_mutex);
			if(lclock.getClockValue() == currentAction.clockVal) {
				cout<< lclock.getClockValue() << currentAction.type << currentAction.param <<endl;
				if(currentAction.type == "TICK") {
					usleep(currentAction.param * 1000);
				}
				else if(currentAction.type == "IDLE") {
					// do some idle action
				}
				else if(currentAction.type == "INIT") {
					// do nothing
				}
				else if(currentAction.type == "SEND") {
					msg->nodeid = nodes[currentAction.param].c_str(); // check if this works without c_str
	//				msg->nodeid = "192.168.1.15";
					sprintf(msg->payLoad, "%d",lclock.getClockValue());
					pthread_create(&send_thread,NULL,send_message,(void *)msg); // async thread call for sending message
				}
				lclock.tick();
				fileInputList.pop_front();
			}

		pthread_mutex_unlock(&clock_mutex);
	}

	pthread_exit(NULL);

}

void* send_message(void *threadarg) {
		int sockfd, numbytes;
		char buf[MAXDATASIZE];
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
		//	return 1;
		}
		// loop through all the results and connect to the first we can
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
