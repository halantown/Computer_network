#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <queue>
#include <stdexcept>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
// #include <netinet/tcp.h>

#define TELNET_PORT 223
#define MAX_USER 2
#define END_STRING "squirrel\n"
#define print_log(...) fprintf(stderr, __VA_ARGS__)

// telnet command
#define IAC_OPT     0b11111111      // 255
#define SE_OPT      0b11110000      // 240
#define IP_OPT      0b11110100      // 244
#define SB_OPT      0b11111010      // 250
#define WILL_OPT    0b11111011      // 251
#define DO_OPT      0b11111101      // 253

// telnet option code
#define TERMINAL_TYPE       24

using namespace std;

void NVT_client();
void get_help();
int connect_server(string ip_addr);
int listening_connection();
int send_message();
int recv_message();
void build_IAC_sequence();

void init_server();
void establish_connection_client();

void set_terminal_type(string type);
inline void trim_str(char* str);
inline void send_IAC_sequence(int fd);


const int BUFFSIZE = 512;
int sockfd, connfd;
string terminal_type;
char send_str[BUFFSIZE];
char recv_buf[BUFFSIZE];
char recv_IAC_buf[BUFFSIZE];
char telnet_cmd_buf[BUFFSIZE];
queue<string> command_buf;
map<int, string> telnet_code;
bool debug = false;
int optval = 1;
// class SimpleException : public exception{
// public:
//     SimpleException(const string& message) : message_(message) {}

//     const char* what() const noexcept override {
//         return message_.c_str();
//     }

// private:
//     string message_; // 存储要打印的字符串
// };


void signalHandler(int signum) {
    print_log("Received signal: Interruption\n");
    recv(connfd, recv_buf, sizeof(recv_buf), MSG_DONTWAIT);
    
}

void close_server(int p){
    close(sockfd); // 关闭服务端 socket
    printf("Close Server\n"); // 输出提示信息
    exit(0); // 正常退出程序
}

int main(int argc, char *argv[])
{
    NVT_client();
    return 0;
}

void init(){
    telnet_cmd_buf[0] = IAC_OPT;

}

void NVT_client()
{
    init();
    while (true)
    {
        printf("telnet_sim> ");
        string input;
        bool validity = true;
        getline(cin, input);
        // auto it = std::find_if(input.rbegin(), input.rend(), [](char ch) { return !std::isspace(ch); });
        // input.erase(it.base(), input.end());
        
        size_t pos = input.find(' '); // 查找第一个空格的位置
        if (pos != string::npos){
            string cmd1 = input.substr(0, pos);
            string cmd2 = input.substr(pos + 1);
            if(cmd1 == "telnet"){
                string num = "";
                int n_num = 0;
                // IP Addr validity check
                for(int i = 0; i < cmd2.length(); i++){
                    if(cmd2[i] == '.') {
                        n_num ++;
                        int part_num = stoi(num);
                        if(part_num < 0 || part_num > 255){
                            validity = false;
                            break;
                        }
                        num = "";
                        continue;
                    }
                    num += cmd2[i];
                }
                if(validity == false || n_num != 3){
                    printf("Invalid IP Addr, format: 0.0.0.0 ~ 255.255.255.255\n");
                    continue;
                }   
                connect_server(cmd2);
            }
        }else{
            if(input == "q" || input == "quit"){
                return;
            }else if (input == "v" || input == "version"){
                printf("This is a telnet protocol simulator by thr, from cs2002 class\n");
            }else if (input == "h" || input == "help"){
                get_help();
            }else if (input == "server"){
                init_server();
            }else if(input == "telnet"){
                printf("usage: \n       telnet IP_ADDR  (Default Port: 223) \n       telnet IP_ADDR PORT\n");
            }else{
                printf("unknown command, type 'h' or 'help' for help\n");
            }
        }
    }
}

void get_help(){
    printf("help...\n");
}

int connect_server(string server_ip){
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd){
        printf("Error creating socket(%d): %s\n", errno, strerror(errno));
        return -1;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TELNET_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if(-1 == connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))){
        printf("Connect error(%d): %s\n", errno, strerror(errno)); 
        return -1;
    }

    establish_connection_client();
    printf("TCP Connection established......\n");
    send_message();
    return 0;
}

int send_message(){
    int recv_pid;
    if((recv_pid = fork())){
        recv_message();
    }else{
        printf("> ");
        while(true){
            bzero(send_str, sizeof(send_str)); // 清空 buff 数组    
            fgets(send_str, BUFFSIZE, stdin);
            if(strncmp(send_str, END_STRING, strlen(END_STRING)) == 0) break;
            if(send_str[0] == '@'){
                // enter command mode
                const char* oob_data = "!";
                send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
            }else if(send(sockfd, send_str, strlen(send_str)+1, 0) < 0){
                // todo error
                printf("error 156");
            }
        }
    }

    kill(recv_pid, SIGKILL);
    exit(0);
}

int recv_message(){
    bzero(recv_buf, sizeof(recv_buf)); // clear char array   
    int filled = 0;
    while((filled = recv(sockfd, recv_buf, BUFFSIZE - 1, 0))){
        recv_buf[filled] = '\0';
        printf("%s", recv_buf);
        fflush(stdout);
        bzero(recv_buf, sizeof(recv_buf)); // clear char array
    }
    // In the right case, 
    // the thread will be force killed by parent thread, instead of terminating correctly.
    printf("Connection closed by remote host");
    return 0;
}

void init_server(){
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd){
        printf("Error creating socket(%d): %s\n", errno, strerror(errno));
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    // setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(int));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(TELNET_PORT);

    // bind port
    if(-1 == ::bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))){
        printf("Bind error(%d): %s\n", errno, strerror(errno));
    }

    // listening socket
    if(-1 == listen(sockfd, MAX_USER)){
        printf("Listen error(%d): %s\n", errno, strerror(errno));
    }

    printf("Server established, waiting clients to connect...\n");
    listening_connection();
}

// server running...
int listening_connection(){

    struct sockaddr_in cli_addr;
    int pid;
    socklen_t cli_len = sizeof(cli_addr);
    signal(SIGINT, close_server);
    signal(SIGURG, signalHandler);
    
    while((connfd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len))){
        if(-1 == connfd){
            printf("Accept error(%d): %s\n", errno, strerror(errno)); // 如果接受失败，输出错误信息
            return -1; // 返回 -1，表示程序异常退出
        }

        if((pid = fork())){
            print_log("Child pid = %d.\n", pid);
        }else{
            // build connection
            pid = getpid();
            print_log("Sub process......\n");
            bool connected = false;
            int len = 0;
            // 
            do{
                bzero(recv_IAC_buf, sizeof(recv_IAC_buf)); // clear char array
                len = recv(connfd, recv_IAC_buf, BUFFSIZE - 1, 0);
            }while(len && (uint8_t)recv_IAC_buf[0] != IAC_OPT);

            for(int i = 0; recv_IAC_buf[i] != '\0' && i < strlen(recv_IAC_buf); i++){
                if((uint8_t)recv_IAC_buf[i] == IAC_OPT){
                    if((uint8_t)recv_IAC_buf[i + 1] == WILL_OPT){
                        // accepting connection, logging.....
                        print_log("Requesting IP_Address: %s:%d\n" , (inet_ntoa(cli_addr.sin_addr)), ntohs(cli_addr.sin_port));
                        connected = true;
                        if((uint8_t)recv_IAC_buf[i + 2] == TERMINAL_TYPE){
                            telnet_cmd_buf[0] = IAC_OPT;
                            telnet_cmd_buf[1] = DO_OPT;
                            telnet_cmd_buf[2] = TERMINAL_TYPE;

                            telnet_cmd_buf[3] = IAC_OPT;
                            telnet_cmd_buf[4] = SB_OPT;
                            telnet_cmd_buf[5] = TERMINAL_TYPE;
                            telnet_cmd_buf[6] = 1;

                            telnet_cmd_buf[7] = IAC_OPT;
                            telnet_cmd_buf[8] = SE_OPT;
                            send_IAC_sequence(connfd);
                            print_log("send <IAC, DO, 24 > <IAC, SB, 24, 1, IAC, SE>\n");
                        }
                    }else 

                    if((uint8_t)recv_IAC_buf[i + 1] == SB_OPT){
                        if(((uint8_t)recv_IAC_buf[i + 2] == TERMINAL_TYPE)){
                            if(((uint8_t)recv_IAC_buf[i + 3] == 0)){
                                int j = 4;
                                string client_terminal_type = "";
                                while((uint8_t)recv_IAC_buf[i + j] < 128){
                                    client_terminal_type += recv_IAC_buf[i + j];
                                    j++;
                                }
                                set_terminal_type(client_terminal_type);
                            }
                        }
                    }else 

                    if(((uint8_t)recv_IAC_buf[i + 1] == SE_OPT)){
                        break;
                    }
                }
            }
                
                
                // if(!(connected)){
                //     print_log("Invalid connection from remote host %s:%d!\n" , 
                //                 inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                //     close(connfd);
                //     exit(-1);
                // }

                printf("child pid = [%d], listening.....\n ", pid);

                dup2(connfd, STDOUT_FILENO);
                setsockopt(connfd, SOL_SOCKET, SO_OOBINLINE, &optval, sizeof(optval));
                fcntl(connfd, F_SETOWN, getpid());

                while(1){
                    int filled = 0;
                    while (true)
                    {
                        len = recv(connfd, recv_buf + filled, BUFFSIZE - filled - 1, 0);
                        if (!len)
                            break;
                        filled += len;
                        if (recv_buf[filled - 1] == '\0')
                            break;
                    }

                    if(!len) {
                        print_log("\t[%d] Client disconnected.\n", pid);
                        break;
                    }
                    
                    trim_str(recv_buf);

                    if((strlen(recv_buf))){
                        // if((uint8_t)recv_buf[0] == IAC_OPT){
                        //     // if kill then kill 

                        // }else{
                            // write into command buffer
                            print_log("\t[%d] Command received: %s\n", pid, recv_buf);
                            system(recv_buf);
                            send(connfd, "> ", 3, MSG_NOSIGNAL);
                        // }
                    }
                    
                }
                close(connfd);
                    exit(0);
            }
            
            // dup2(connfd, STDERR_FILENO);

            


            // start communicating
            // while(true){ 
            //     // parent process server_run_process
            //     int pipefd[2], pipefd_exe[2], filled = 0;
            //     if(pipe(pipefd) == -1 || pipe(pipefd_exe) == -1){
            //         print_log("Failed to create pipe \n");
            //     }

            //     int recv_ctrl_pid = fork();

            //     if (!( recv_ctrl_pid ))
            //     {
            //         // recv_ctrl --> fork subprocesses
            //         int command_pid = fork();
            //         if(!(command_pid)){
            //             // command_process
            //             while(true){
            //                 ssize_t count = read(pipefd[0], recv_buf, sizeof(recv_buf));
            //                 // command_buf.push(string(recv_buf, 512));
            //                 write(pipefd_exe[1], recv_buf, sizeof(recv_buf));
            //             }
                        
            //         }else{
            //             int exec_pid = fork();
            //             if(!(exec_pid)){
            //                 // exec_process
            //                 while(true){
            //                     // exe_pid - child-child process
            //                     ssize_t count = read(pipefd_exe[0], recv_buf, sizeof(recv_buf));

            //                     if (count == -1) {
            //                         print_log("Failed to read from pipe\n");
            //                         return 1;
            //                     } else if (count == 0) {
            //                         break; // end of stream
            //                     } else {
            //                         print_log("\t[%d] Command executing: %s\n", pid, recv_buf);
            //                         system(recv_buf);
            //                         // write(pipefd_exe[1], "done", 4);
            //                     }
            //                     send(connfd, "> ", 3, MSG_NOSIGNAL);
            //                 }
            //             }else{
            //                 // recv_ctl core 
            //                 close(pipefd[0]);
            //                 while (true){
            //                 // receive from remote client
            //                     while(true){
            //                         len = recv(connfd, recv_buf+filled, BUFFSIZE-filled-1, 0);
            //                         if(!len) break;
            //                         filled += len;
            //                         if(recv_buf[filled-1] == '\0') break;
            //                     }

            //                     if(!len) {
            //                         print_log("\t[%d] Client disconnected.\n", pid);
            //                         break;
            //                     }
                                
            //                     trim_str(recv_buf);

            //                     if((strlen(recv_buf))){
            //                         if((uint8_t)recv_buf[0] == IAC_OPT){
            //                             // if kill then kill 

            //                         }else{
            //                             // write into command buffer
            //                             print_log("\t[%d] Command received: %s\n", pid, recv_buf);
            //                             write(pipefd[1], recv_buf, strlen(recv_buf));
            //                         }
            //                     }
            //                 }
            //             }
            //         }
            //     }
            // }
            // close(connfd);
			// print_log("\t[%d] Dying.", pid);
			// abort();
        

        // bzero(send_str, BUFFSIZE);
        // recv(connfd, send_str, BUFFSIZE - 1, 0);
        // printf("Recv: %s\n", send_str);
        // send(connfd, send_str, strlen(send_str), 0);
        // close(connfd);
    }
    return 0;
}

inline void send_IAC_sequence(int fd){
    if(send(fd, telnet_cmd_buf, strlen(telnet_cmd_buf)+1, 0) < 0){
        // todo error
        printf("error 156");
    }
}

inline void trim_str(char* str){
    int l = 0, r = strlen(str) - 1;
    
    if(str[r] == '\n')  r --;
    while(str[l] == ' ')    l++;
    while(str[r] == ' ')    r--;

    for(int i = l; i < r; i ++)
        str[i - l] = str[i];
    str[r - l + 1] = '\0';
}

void establish_connection_client(){

    bool connected = false;

    telnet_cmd_buf[1] = WILL_OPT;
    telnet_cmd_buf[2] = TERMINAL_TYPE;
    printf("send <IAC, WILL, 24>\n");
    fflush(stdout);
    send_IAC_sequence(sockfd);
    int len = 1;
    do{
        bzero(recv_IAC_buf, sizeof(recv_IAC_buf)); // clear char array
        len = recv(sockfd, recv_IAC_buf, BUFFSIZE - 1, 0);
    }while(len && (uint8_t)recv_IAC_buf[0] != IAC_OPT);
    
    for(int i = 0; recv_IAC_buf[i] != '\0' && i < strlen(recv_IAC_buf); i++){
        if((uint8_t)recv_IAC_buf[i] == IAC_OPT){
            if((uint8_t)recv_IAC_buf[i + 1] == DO_OPT){
                connected = true;
                if(((uint8_t)recv_IAC_buf[i + 2] == TERMINAL_TYPE)){
                    
                }
            }else 

            if((uint8_t)recv_IAC_buf[i + 1] == SB_OPT){
                if(((uint8_t)recv_IAC_buf[i + 2] == TERMINAL_TYPE)){
                    if(((uint8_t)recv_IAC_buf[i + 3] == TERMINAL_TYPE)){
                        // send terminal type
                        telnet_cmd_buf[0] = IAC_OPT;
                        telnet_cmd_buf[1] = SB_OPT;
                        telnet_cmd_buf[2] = TERMINAL_TYPE;

                        telnet_cmd_buf[3] = 0;
                        telnet_cmd_buf[4] = 'M';
                        telnet_cmd_buf[5] = 'A';
                        telnet_cmd_buf[6] = 'C';

                        telnet_cmd_buf[7] = IAC_OPT;
                        telnet_cmd_buf[8] = SE_OPT;
                        send_IAC_sequence(sockfd);
                        printf("send <IAC, SB, 24, 0, 'M', 'A', 'C', IAC, SE>\n");
                    }
                }

            }else 

            if(((uint8_t)recv_IAC_buf[i + 1] == SE_OPT)){
                break;
            }
        }
    }

    if(!connected){
        printf("error 1");
    }
    return;

}

void set_terminal_type(string type){
    terminal_type = type;
}