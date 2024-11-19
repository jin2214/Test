#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int client_sockets[MAX_CLIENTS]; //10명 까지 들어올 수 있음, 소켓 디스크립터 배열
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //쓰레드 간에 공유 데이터 동기화를 위해 사용, mutex초기화

void *handle_client(void *arg) {
    //인자는 클라이언트 소켓 디스크립터 주소
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        //클라이언트 소켓으로부터 send된 데이터를 수신, 성공 시 읽은 바이트 수를 리턴, 데이터 들어올 때까지 대기(블로킹모드)
        //클라이언트가 연결을 종료하면 0이 되고 while문 빠져나감
        buffer[bytes_read] = '\0';
        printf("Received: %s", buffer); // 서버 터미널에 출력

        pthread_mutex_lock(&mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0 && client_sockets[i] != client_socket) {
                //클라이언트 소켓이 할당되어 있는 만큼, 자기 자신 클라이언트에게는 전송안함
                send(client_sockets[i], buffer, strlen(buffer), 0); //메세지를 다른 클라이언트에게 전송
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = 0; //연결이 종료된 클라이언트 소켓의 배열을 0으로 바꿈
            printf("A client is disconnected\n");
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, new_socket; //서버 소켓, 클라이언트 소켓 디스크립터
    struct sockaddr_in server_addr, client_addr; //소켓을 바인드 할 때 특성으로 넣을 구조체(?), 소켓의 주소 정보가 저장되는 구조체
    socklen_t addr_len = sizeof(client_addr); //위 구조체 크기

    server_socket = socket(AF_INET, SOCK_STREAM, 0); //서버 소켓 만들기, IPv4 프로토콜, TCP방식
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; //주소체계 설정, IPv4 프로토콜 사용하겠다는 뜻
    server_addr.sin_addr.s_addr = INADDR_ANY; //소켓을 바인딩(특정 IP주소, 포트 번호에 연결), 모든 주소에 바인딩할 수 있도록 함
    server_addr.sin_port = htons(PORT); //서버가 사용할 포트 결정, 바이트 순서를 네트워크 통신에 맞게 변경하는 함수

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) { //서버 소켓을 수신 대기 상태로 전환, backlog는 대기열의 최대크기
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len); //서버 소켓 대기열에 연결 요청이 있으면 새로운 소켓으로 할당(클라이언트 요청)
        //대기열에 연결 요청이 없는 경우 block모드에서 프로세스 대기
        if (new_socket == -1) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&mutex); //공유자원을 동기화 하기 위해 mutex를 잠금
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                break; //0번째 클라이언트 소켓부터 비어있으면 소켓 디스크립터를 할당 후 break
            }
        }
        pthread_mutex_unlock(&mutex); //mutex 잠금해제

        if (i == MAX_CLIENTS) { 
            //클라이언트가 최대가 된 경우 연결 거부, 소켓 제거
            printf("Maximum clients connected. Connection refused.\n");
            close(new_socket);
        } else {
            //각 클라이언트 소켓 디스크립터에 쓰레드를 할당
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, (void *)&new_socket); //handle_client 함수를 독립적으로 사용하는 쓰레드 생성, 함수에 전달할 인자 설정
            pthread_detach(thread); //쓰레드를 독립적으로 실행되도록 함, 종료 시 리소스 자동 반환
            printf("New client connected.\n");
        }
    }

    close(server_socket);
    return 0;
}
