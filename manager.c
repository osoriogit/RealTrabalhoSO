
#include "util.h"

#define SERVER_PIPE "server"
#define CLIENT_FIFO_TEMPLATE "cliente_%d"

Topics topics[MAX_TOPICS];
Users users[MAX_USERS];
int num_topics = 0;
int num_clients = 0;
// Variáveis globais
char client_pipe_name[20];
int clientes_conectados = 0;
int running = 1; // Controla a execução
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


// Função para criar a variável de ambiente MSG_FICH
int createMsgFich(char *msg_fich) {
    if (setenv("MSG_FICH", "mensagem.txt", 1) == -1) {
        perror("[ERRO] MSG_FICH nao foi criada\n");
        return 0;
    }
    msg_fich = getenv("MSG_FICH");

    if (msg_fich) {
        printf("[INFO] MSG_FICH foi criada\n");
        return 1;
    } else {
        printf("[ERRO] MSG_FICH nao foi criada\n");
        return 0;
    }
}
// Função para encontrar ou criar um tópico
int findOrCreateTopic(const char *topic_name) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].nome, topic_name) == 0) {
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    if (num_topics < MAX_TOPICS) {
        strncpy(topics[num_topics].nome, topic_name, MAX_TOPIC_NAME);
        topics[num_topics].nPersistent = 0;
        topics[num_topics].isBlocked = 0;
        num_topics++;
        pthread_mutex_unlock(&mutex);
        return num_topics - 1;
    }
    pthread_mutex_unlock(&mutex);
    return -1; // Não foi possível criar mais tópicos
}

// Função para subscrever um cliente a um tópico
int subscribeClient(const char *username, const char *topic_name) {
    fprintf(stderr, "[DEBUG] %s, %s, %d, %s\n", username,topic_name);
    pthread_mutex_lock(&mutex);

    int topic_idx = findOrCreateTopic(topic_name);
    if (topic_idx == -1) {
        pthread_mutex_unlock(&mutex);
        return -1; // Tópico não encontrado ou não criado
    }

    if (topics[topic_idx].isBlocked) {
        pthread_mutex_unlock(&mutex);
        return -2; // Tópico bloqueado
    }

    for (int i = 0; i < num_clients; i++) {
        if (strcmp(users[i].username, username) == 0) {
            for (int j = 0; j < users[i].nSubs; j++) {
                if (strcmp(users[i].subs[j], topic_name) == 0) {
                    pthread_mutex_unlock(&mutex);
                    return 0; // Já está subscrito
                }
            }
            if (users[i].nSubs < MAX_TOPICS) {
                strncpy(users[i].subs[users[i].nSubs], topic_name, MAX_TOPIC_NAME);
                users[i].nSubs++;
            }
            pthread_mutex_unlock(&mutex);
            return 1; // Sucesso
        }
    }
    // Adicionar novo cliente
    if (num_clients < MAX_USERS) {
        strncpy(users[num_clients].username, username, MAX_USERNAME_SIZE);
        strncpy(users[num_clients].subs[0], topic_name, MAX_TOPIC_NAME);
        users[num_clients].nSubs = 1;
        num_clients++;
        pthread_mutex_unlock(&mutex);
        return 1; // Sucesso
    }

    pthread_mutex_unlock(&mutex);
    return -3; // Falha ao adicionar cliente
}
// Função para bloquear ou desbloquear um tópico
int blockTopic(const char *topic_name, int block) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].nome, topic_name) == 0) {
            topics[i].isBlocked = block;
            pthread_mutex_unlock(&mutex);
            return 1; // Sucesso
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1; // Tópico não encontrado
}
void broadcastMessage(const char *topic_name, const char *message) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_clients; i++) {
        for (int j = 0; j < users[i].nSubs; j++) {
            if (strcmp(users[i].subs[j], topic_name) == 0) {
                snprintf(client_pipe_name, sizeof(client_pipe_name), CLIENT_FIFO_TEMPLATE, atoi(users[i].username));
                int client_fd = open(client_pipe_name, O_WRONLY);
                if (client_fd != -1) {
                    write(client_fd, message, strlen(message) + 1);
                    close(client_fd);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}
// Função para lidar com clientes
void *handleClients(void *arg) {
    int server_fd = *((int *)arg);
    Pedido pedido;

    while (running) {
        read(server_fd, &pedido, sizeof(pedido));
            //fprintf(stderr, "[DEBUG] %s, %s, %d, %s\n",
                   // pedido.acao, pedido.topico, pedido.duracao, pedido.mensagem);
            
            if (strcmp(pedido.acao, "validar") == 0) {
                clientes_conectados++;
                printf("USERNAME INTRUDUZIDO %s\n",pedido.mensagem);
                Resposta message;
                strcpy(message.mensagem,"USER VALIDO");
                strcpy(message.motivo,"USER VALIDO");
                if(clientes_conectados>10){strcpy(message.motivo,"USER_LIMIT");};
                int client_fd = open(pedido.mensagem, O_WRONLY);
                if (client_fd != -1) {
                    write(client_fd, &message, sizeof(message));
                    close(client_fd);
                }
                if(clientes_conectados<10){strcpy(users[clientes_conectados].username,pedido.mensagem);};
                strcpy(pedido.acao, ""); 
            }
            if (strcmp(pedido.acao, "subscribe") == 0) {
                int result = subscribeClient(pedido.username, pedido.topico);
                if (result == -2) {
                    printf("[INFO] Falha: Tópico %s está bloqueado.\n", pedido.topico);
                } else if (result > 0) {
                    printf("[INFO] %s subscreveu ao tópico %s\n", pedido.username, pedido.topico);
                }
            } else if (strcmp(pedido.acao, "msg") == 0) {
                printf("[INFO] Mensagem recebida no tópico %s: %s\n",
                       pedido.topico, pedido.mensagem);
                broadcastMessage(pedido.topico, pedido.mensagem);
            }
    }

    close(server_fd); // Fecha o descritor de arquivo do cliente.
    return NULL;
}

// Função para lidar com comandos do administrador
void *handleAdmin(void *arg) {
    char buffer[256];

    while (running) {
        printf("Comando: ");
        fgets(buffer, sizeof(buffer), stdin);

        if (strncmp(buffer, "subscribe", 9) == 0) {
            char *username = strtok(buffer + 10, " ");
            char *topic_name = strtok(NULL, "\n");
            if (username && topic_name) {
                int result = subscribeClient(username, topic_name);
                if (result == -2) {
                    printf("[INFO] Falha: Tópico %s está bloqueado.\n", topic_name);
                } else if (result > 0) {
                    printf("[INFO] %s subscrito ao tópico %s pelo admin\n", username, topic_name);
                }
            }
        } else if (strncmp(buffer, "block", 5) == 0) {
            char *topic_name = strtok(buffer + 6, "\n");
            if (topic_name) {
                if (blockTopic(topic_name, 1) > 0) {
                    printf("[INFO] Tópico %s bloqueado.\n", topic_name);
                } else {
                    printf("[INFO] Falha ao bloquear o tópico %s.\n", topic_name);
                }
            }
        } else if (strncmp(buffer, "unblock", 7) == 0) {
            char *topic_name = strtok(buffer + 8, "\n");
            if (topic_name) {
                if (blockTopic(topic_name, 0) > 0) {
                    printf("[INFO] Tópico %s desbloqueado.\n", topic_name);
                } else {
                    printf("[INFO] Falha ao desbloquear o tópico %s.\n", topic_name);
                }
            }
        } else if (strcmp(buffer, "close\n") == 0) {
            printf("[INFO] Encerrando o servidor...\n");
            pthread_mutex_lock(&mutex);
            running = 0; // Sinaliza para encerrar
            pthread_mutex_unlock(&mutex);
        }
    }

    return NULL;
}

// Função para gerenciar temporizadores
void *handleTimer(void *arg) {
    while (running) {
        sleep(5); // Temporizador de 5 segundos
    }

    return NULL;
}

int main() {
    char *msg_fich;
    if (createMsgFich(msg_fich) == 0) {
        printf("[ERRO] Erro ao criar armazenamento de texto\n");
        return -505;
    }

    // Cria o FIFO do servidor
    if (mkfifo(SERVER_PIPE, 0666) == -1) {
        perror("[ERRO] Erro ao criar pipe do servidor");
        return -1;
    }
    printf("[INFO] Servidor pronto.\n");

    int server_fd = open(SERVER_PIPE, O_RDONLY | O_NONBLOCK);
    if (server_fd == -1) {
        perror("[ERRO] Erro ao abrir o pipe do servidor");
        unlink(SERVER_PIPE);
        return 1;
    }

    // Criação das threads
    pthread_t client_thread, admin_thread, timer_thread;

    if (pthread_create(&client_thread, NULL, handleClients, (void *)&server_fd) != 0) {
        perror("[ERRO] Erro ao criar thread de clientes");
        close(server_fd);
        unlink(SERVER_PIPE);
        return 1;
    }

    if (pthread_create(&admin_thread, NULL, handleAdmin, NULL) != 0) {
        perror("[ERRO] Erro ao criar thread de administrador");
        close(server_fd);
        unlink(SERVER_PIPE);
        return 1;
    }

    if (pthread_create(&timer_thread, NULL, handleTimer, NULL) != 0) {
        perror("[ERRO] Erro ao criar thread de temporizador");
        close(server_fd);
        unlink(SERVER_PIPE);
        return 1;
    }

    // Aguarda as threads terminarem
    pthread_join(client_thread, NULL);
    pthread_join(admin_thread, NULL);
    pthread_join(timer_thread, NULL);

    close(server_fd);
    unlink(SERVER_PIPE);
    printf("[INFO] Servidor encerrado.\n");

    return 0;
}
