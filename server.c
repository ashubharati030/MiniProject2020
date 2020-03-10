#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<sqlite3.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#define SA struct sockaddr 
const int maxsqlcmd=1000;
const int comdsize=10;
char username[10],password[10],filename[10];
int count=0;
char fileDetails[10000],filecontent[1000000];
void MAIN(sqlite3*,int);

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	char string[1000];
	for(i=0;i<argc;i++){
		bzero(string,1000);
		sprintf(string,"%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
		strcat(fileDetails,string);
	}
	strcat(fileDetails,"\n");
	return 0;
}

static int callbackfile(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	char string[1000];
	for(i=0;i<argc;i++){
		bzero(string,1000);
		sprintf(string,"%s\n",argv[i] ? argv[i]: "NULL");
		strcat(filecontent,string);
	}
	strcat(filecontent,"\n");
	return 0;
}

static int callback_verify(void *NotUsed, int argc, char **argv, char **azColName){

        int i;
        for(i=0;i<argc;i++){
                count++;
        }
        return 0;
}

bool issame(char s1[],char s2[],int size){
	for(int i=0;i<size;i++){
		if(s1[i]!=s2[i]){
			return false;
		}
	}
	return true;
}

bool USR_PASS(sqlite3 *db,int sockfd){
	char *errmsg=0;
	char sql[maxsqlcmd];
	bzero(username,10);
	bzero(password,10);
	read(sockfd,username,10);
	read(sockfd,password,10);
	username[8]='\0';
	password[8]='\0';
	sprintf(sql,"SELECT Id FROM LoginDetails WHERE UserName='%s' AND Password='%s';",username,password);
        count=0;
        sqlite3_exec(db,sql,callback_verify,0,&errmsg);
	if(count>0){
		write(sockfd,"1",1);
		return true;
	}
	write(sockfd,"0",1);
	return false;
}

bool USR(sqlite3* db,int sockfd)
{
	char *errmsg=0;
	char sql[maxsqlcmd];
	bzero(username,10);bzero(password,10);
	read(sockfd,username,10);
	read(sockfd,password,10);
	username[8]='\0';
	password[8]='\0';
	sprintf(sql,"SELECT Id FROM LoginDetails WHERE UserName='%s';",username);
        count=0;
        sqlite3_exec(db,sql,callback_verify,0,&errmsg);
	if(count>0){
		write(sockfd,"0",1);
		return false;
	}
	bzero(sql,maxsqlcmd);
	sprintf(sql, "INSERT INTO LoginDetails(UserName,Password) VALUES('%s','%s');",username, password);
        sqlite3_exec(db,sql,0,0,&errmsg);
	write(sockfd,"1",1);
	return true;
}

void USR_FL(sqlite3 *db,int sockfd){
	
	char filesize[8],*textfile,reciver[10];
	int size;
	char *errmsg=0;
	char sql[maxsqlcmd];
	bzero(reciver,10);
        read(sockfd,reciver,10);
        reciver[8]='\0';
	bzero(filename,10);
	read(sockfd,filename,10);
	filename[8]='\0';
	bzero(filesize,8);
	read(sockfd,filesize,8);
	filesize[6]='\0';
	size=atoi(filesize);
	textfile=(char*)malloc((size+2)*sizeof(char));
	read(sockfd,textfile,size+2);
	textfile[size+1]='\0';	
	bzero(sql,maxsqlcmd);
	sprintf(sql,"SELECT Id FROM LoginDetails WHERE UserName='%s';",reciver);
        count=0;
        sqlite3_exec(db,sql,callback_verify,0,&errmsg);
	if(count>0){
		bzero(sql,maxsqlcmd);
		sprintf(sql,"INSERT INTO Graph(Sender,Reciver,Title,Size,File) VALUES ('%s','%s','%s','%d','%s');",username,reciver,filename,size,textfile);
		sqlite3_exec(db,sql,0,0,&errmsg);
		write(sockfd,"1",1);
		MAIN(db,sockfd);
		return;
	}
	write(sockfd,"0",1);
	MAIN(db,sockfd);

}

void DWN(sqlite3 *db,int sockfd){

	char sql[maxsqlcmd];
	char buffer[8];
	int id,RC;
	char *errmsg=0; 
	bzero(buffer,8);
	read(sockfd,buffer,8);
	buffer[6]='\0';
	id=atoi(buffer);
	bzero(sql,maxsqlcmd);
	sprintf(sql,"SELECT File FROM Graph WHERE Id='%d';",id);
	RC=sqlite3_exec(db,sql,callbackfile,0,&errmsg);
	if(filecontent[0]=='\0'){
		write(sockfd,"0",1);
		return ;
	}
	write(sockfd,filecontent,sizeof(filecontent));
	bzero(filecontent,sizeof(filecontent));
	write(sockfd,"1",1);
}

void DEL(sqlite3*db,int sockfd){
	
	char sql[maxsqlcmd];
 	char buffer[8];
        int id,RC;
	char *errmsg=0;
        bzero(buffer,8);
        read(sockfd,buffer,8);
        buffer[6]='\0';
        id=atoi(buffer);
        bzero(sql,maxsqlcmd);
        sprintf(sql,"DELETE FROM Graph WHERE Id='%d' AND Reciver='%s';",id,username);
        RC=sqlite3_exec(db,sql,0,0,&errmsg);
}

void FL_D(sqlite3 *db,int sockfd){
	
	char sql[maxsqlcmd];
	char buffer[8];
	char *errmsg;
	sprintf(sql,"SELECT Id, Sender, Title, Size FROM Graph WHERE Reciver='%s';",username);
	sqlite3_exec(db,sql,callback,0,&errmsg);
	write(sockfd,fileDetails,sizeof(fileDetails));
	bzero(fileDetails,sizeof(fileDetails));
	bzero(buffer,8);
	read(sockfd,buffer,8);
	buffer[6]='\0';
	if(issame(buffer,"DWN_FL",6)){
	
		DWN(db,sockfd);
	}
	else if(issame(buffer,"DEL_FL",6)){
	
		DEL(db,sockfd);
	}
	
	MAIN(db,sockfd);

}

void MAIN(sqlite3 *db,int sockfd){
	char *errmsg=0;
        char buffer[comdsize-2];
        bzero(buffer,comdsize-2);
        read(sockfd,buffer,comdsize-2);
	buffer[comdsize-4]='\0';
	if(issame(buffer,"LOGOUT",comdsize-4)){
		close(sockfd);
	}
	else if(issame(buffer,"SND_FL",comdsize-4)){
		
		USR_FL(db,sockfd);		
	}
	else if(issame(buffer,"EXTRCT",comdsize-4)){
	
		FL_D(db,sockfd);
	}

}
void START(sqlite3* db,int sockfd)
{
	int readcount;
	char *errmsg=0;
	char buffer[comdsize+2];	//two places one for null and other for carriage return.
	bzero(buffer,comdsize+2);
	readcount=read(sockfd,buffer,comdsize+2);
	buffer[comdsize]='\0';
	printf("%s %d\n",buffer,readcount);
	if(issame(buffer,"VERIFY_USR",comdsize)){
		bool value=USR_PASS(db,sockfd);
		if(value){
			MAIN(db,sockfd);
		}
		else
		{
			START(db,sockfd);
		}
	}
	else if(issame(buffer,"UNIQUE_USR",comdsize)){
		bool value=USR(db,sockfd);
		START(db,sockfd);
	}
	else{
		START(db,sockfd);
	}
}
void finalfunction(int sockfd){ 
	sqlite3 *db;
        sqlite3_open("UserDetails.db",&db);
	char sql[maxsqlcmd],*errmsg=0;
	sprintf(sql, "CREATE TABLE LoginDetails(Id INTEGER PRIMARY KEY, UserName TEXT UNIQUE, Password TEXT);");
        sqlite3_exec(db,sql,0,0,&errmsg);
	bzero(sql,maxsqlcmd);
	sprintf(sql,"CREATE TABLE Graph(Id INTEGER PRIMARY KEY, Sender TEXT,Reciver TEXT,Title TEXT,Size INTEGER,File TEXT);");
        sqlite3_exec(db,sql,0,0,&errmsg);
	START(db,sockfd);
}


int main(int argc, char *argv[]){
	if(argc<2){
                fprintf(stderr, "Port No not provided. Program Terminated.\n");
                exit(1);
        }
	int sockfd, connfd, len;
   	struct sockaddr_in servaddr, cli;  //structure sockaddr_in is already prst in sys/
  
    	// socket create and verification 
   	sockfd = socket(AF_INET, SOCK_STREAM, 0); //server creation.
    	if (sockfd == -1) { 
        	printf("socket creation failed...\n"); 
        	exit(0); 
    	}	 
    	else
        	printf("Socket successfully created..\n"); 
    	bzero(&servaddr, sizeof(servaddr)); 
  
    	// assign IP, PORT 
    	servaddr.sin_family = AF_INET; 			//ipp4 -- ip type
    	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   //ip address
    	servaddr.sin_port =htons(atoi(argv[1])); 	//port
  
    	// Binding newly created socket to given IP and verification 
    	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {	//SA there at top in macro.
        	printf("socket bind failed...\n"); 
        	exit(0); 
    	} 
    	else
        	printf("Socket successfully binded..\n"); 
  
    	// Now server is ready to listen and verification 
    	if ((listen(sockfd, 5)) != 0) { 
        	printf("Listen failed...\n"); 
        	exit(0); 
    	} 
    	else
        	printf("Server listening..\n"); 
    	len = sizeof(cli); 
  
    	// Accept the data packet from client and verification 
    	connfd = accept(sockfd, (SA*)&cli, &len); 
    	if (connfd < 0) { 
        	printf("server acccept failed...\n"); 
        	exit(0); 
    	} 
    	else{
        	printf("server acccept the client...\n"); 
		}


    	finalfunction(connfd);
        
	return 0;
}
