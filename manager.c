
#include "util.h"

#define SERVER_PIPE "server"
#define CLIENT_FIFO_TEMPLATE "cliente_%d"

Topics topics[MAX_TOPICS];
Users users[MAX_USERS];
//TOPICOS
int num_topics = 0;
int num_clients = 0;//clientes topicos
// Variáveis globais
char client_pipe_name[20];
int clientes_conectados = 0;
int running = 1; // Controla a execução
//STOP WAIT UNTIL OTHER
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//
int testeIgualFunc(const char *str, int tamanho_array) {
    for (int i = 0; i < tamanho_array; i++) {
        if (strncmp(str, users[i].username,5) == 0) {
            return 1;
        }
    }
    return 0;
}
// criar a variável de ambiente MSG_FICH
int createMsgFich(char *msg_fich) {
    if (setenv("MSG_FICH", "mensagem.txt", 1) == -1) {
        return 0;
    }
    msg_fich = getenv("MSG_FICH");

    if (msg_fich) {
        printf("var ambiente foi criada\n");
        return 1;
    } else {
        printf("var ambiente nao foi criada\n");
        return 0;
    }
}
// find or create topico
int findOrCreateTopic(const char *topic_name) {
    //fprintf(stderr, "[DEBUG] ENTROU NO BLOCO MUTEX");

    for (int i = 0; i < num_topics; i++) {
       // fprintf(stderr, "[DEBUG] ENTROU NO IF %s, %s", topics[i].nome, topic_name);

        if (strcmp(topics[i].nome, topic_name) == 0) {
            fprintf(stderr, "[DEBUG] topico ja existe");
            return i;
        }
    }
    if (num_topics < MAX_TOPICS) {
        //fprintf(stderr, "[DEBUG] ENTROU NO IF 2 %i", num_topics);

        strncpy(topics[num_topics].nome, topic_name, MAX_TOPIC_NAME);
        topics[num_topics].nPersistent = 0;
        topics[num_topics].isBlocked = 0;
        num_topics++;
        return num_topics - 1;
    }
    return -1; // nao deu para criar mais tópicos
}

// subscribe cliente a um topico
int subscribeClient(const char *username, const char *topic_name) {
    pthread_mutex_lock(&mutex);

    int topic_idx = findOrCreateTopic(topic_name);


    if (topic_idx == -1) {
        pthread_mutex_unlock(&mutex);
        return -1; // nao encontrado ou nao criado
    }

    if (topics[topic_idx].isBlocked) {
        pthread_mutex_unlock(&mutex);
        return -2; // topico bloqueado
    }

    for (int i = 0; i < num_clients; i++) {
        if (strcmp(users[i].username, username) == 0) {
            for (int j = 0; j < users[i].nSubs; j++) {
                if (strcmp(users[i].subs[j], topic_name) == 0) {
                    pthread_mutex_unlock(&mutex);
                    return 0; // ja estava subscrito
                }
            }
            if (users[i].nSubs < MAX_TOPICS) {
                strncpy(users[i].subs[users[i].nSubs], topic_name, MAX_TOPIC_NAME);
                users[i].nSubs++;
            }
            pthread_mutex_unlock(&mutex);
            return 1; // sucesso
        }
    }
    // tentar adicionar cliente
    if (num_clients < MAX_USERS) {
        strncpy(users[num_clients].username, username, MAX_USERNAME_SIZE);
        strncpy(users[num_clients].subs[0], topic_name, MAX_TOPIC_NAME);
        users[num_clients].nSubs = 1;
        num_clients++;
        pthread_mutex_unlock(&mutex);
        return 1; // sucesso a adicionar cliente
    }

    pthread_mutex_unlock(&mutex);
    return -3; // falha ao adicionar cliente
}
// bloquear ou desbloquear um topico
int blockTopic(const char *topic_name, int block) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].nome, topic_name) == 0) {
            topics[i].isBlocked = block;
            pthread_mutex_unlock(&mutex);
            return 1; // sucesso
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1; // topico nao encontrado
}

void broadcastMessage(const char *topic_name, Resposta message) {
    //fprintf(stderr, "[DEBUG] BROADCAST_ENTER");
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_clients; i++) {
        for (int j = 0; j < users[i].nSubs; j++) {
            if (strcmp(users[i].subs[j], topic_name) == 0){
                strcpy(client_pipe_name, users[i].username);
                int client_fd = open(client_pipe_name, O_WRONLY);
                if (client_fd != -1) {
                    write(client_fd, &message, sizeof(message));
                    close(client_fd);
                }
            }
        }
    }
    //fprintf(stderr, "[DEBUG] BC_CLOSE");

    pthread_mutex_unlock(&mutex);
}
void broadcastALL(Resposta message) {
    //fprintf(stderr, "[DEBUG] BROADCAST_ENTER");
    for (int i = 0; i < 10; i++) {
            strcpy(client_pipe_name, users[i].username);
            int client_fd = open(client_pipe_name, O_WRONLY);
            if (client_fd != -1) {
                write(client_fd, &message, sizeof(message));
                close(client_fd);
            }
    }
    //fprintf(stderr, "[DEBUG] BC_CLOSE");
}
// Thread lidar com clientes
void *handleClients(void *arg) {
    int server_fd = *((int *)arg);
    Resposta message;
    Pedido pedido;
    while (running) {
        read(server_fd, &pedido, sizeof(pedido));
            //VALIDAR
            if (strcmp(pedido.acao, "validar") == 0) {
                    printf("USERNAME INTRUDUZIDO %s\n",pedido.mensagem);
                    strcpy(message.mensagem,"USER VALIDO");
                    strcpy(message.motivo,"USER VALIDO");

                    if(clientes_conectados>10){strcpy(message.motivo,"USER_LIMIT");};

                    int testeIgual=testeIgualFunc(pedido.mensagem,clientes_conectados);

                    if(testeIgual==1){strcpy(message.motivo,"USER_ALR_EXISTS");};
                    int client_fd = open(pedido.mensagem, O_WRONLY);
                    if (client_fd != -1) {
                        write(client_fd, &message, sizeof(message));
                        close(client_fd);
                    }
                    if(clientes_conectados<10){strcpy(users[clientes_conectados].username,pedido.mensagem);clientes_conectados++;};
                    strcpy(pedido.acao, ""); 
            }
            //SUBSCRIBE
            if (strcmp(pedido.acao, "subscribe") == 0) {
                int result = subscribeClient(pedido.username, pedido.topico);
                if (result == -2) {
                    printf("[INFO] Falha: Tópico %s está bloqueado.\n", pedido.topico);
                } else if (result > 0) {
                    printf("[INFO] %s subscreveu ao tópico %s\n", pedido.username, pedido.topico);
                    int v = open(pedido.username, O_WRONLY);
                     
                    sprintf(message.mensagem, "SUBSCREVEU AO TOPICO %s", pedido.topico);
                    pthread_mutex_lock(&mutex);
                    strcpy(message.motivo,"Sucesso subscrito");
                    write(v, &message, sizeof(message));
                    close(v); 
                    pthread_mutex_unlock(&mutex);
                }
                strcpy(pedido.acao, "");
            }else if (strcmp(pedido.acao, "unsubscribe") == 0) {
                    for (int i = 0; i < num_clients; i++) {
                        if (strcmp(users[i].username,pedido.username) == 0) {
                            if (users[i].nSubs > 0) {
                                strncpy(users[i].subs[users[i].nSubs], "", MAX_TOPIC_NAME);
                                users[i].nSubs--;
                                {
                                    printf("[INFO] %s dessubscreveu ao tópico %s\n", pedido.username, pedido.topico);
                                    int v = open(pedido.username, O_WRONLY);
                                    
                                    sprintf(message.mensagem, "DESUBSCREVEU AO TOPICO %s", pedido.topico);
                                    pthread_mutex_lock(&mutex);
                                    strcpy(message.motivo,"Sucesso dessubscrito");
                                    write(v, &message, sizeof(message));
                                    close(v); 
                                    pthread_mutex_unlock(&mutex);
                                }
                            }
                        }
                    }strcpy(pedido.acao, "");

                } else if (strcmp(pedido.acao, "topics") == 0) {{
                        
                        printf("\nTopicos:\n");
                        pthread_mutex_lock(&mutex);
                        for (int i = 0; i < num_topics; i++) {
                            printf(" Nome: %s\n", topics[i].nome);
                            strcpy(message.mensagem,topics[i].nome);
                            strcpy(message.motivo,topics[i].nome);
                            int client_fd = open(pedido.username, O_WRONLY);
                            if (client_fd != -1) {
                                write(client_fd, &message, sizeof(message));
                                close(client_fd);
                            }
                        }
                        pthread_mutex_unlock(&mutex);
                }
                strcpy(pedido.acao, ""); 

            } else if (strcmp(pedido.acao, "msg") == 0) {
                printf("[INFO] Mensagem recebida no tópico %s: %s\n",
                       pedido.topico, pedido.mensagem);

                strcpy(message.mensagem,pedido.mensagem);
                strcpy(message.motivo,"Broadcast");
                broadcastMessage(pedido.topico, message);
                strcpy(pedido.acao, ""); 

            }else if (strcmp(pedido.acao, "exit") == 0) {
                clientes_conectados--;
                printf(" %s Vai Sair\n",pedido.username);
                strcpy(message.mensagem,"xau vais sair");
                strcpy(message.motivo,"USER_LEAVE");
                int client_fd = open(pedido.username, O_WRONLY);
                if (client_fd != -1) {
                    write(client_fd, &message, sizeof(message));
                    close(client_fd);
                    strcpy(pedido.acao, "");
                }
            }
    }

    close(server_fd);
    return NULL;
}

// Thread comandos do administrador
void *handleAdmin(void *arg) {
    char buffer[256];
    Resposta message;
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
        }else if (strncmp(buffer, "remove", 6) == 0) {
            char *usernamekick = strtok(buffer + 7, "\n");
            strcpy(message.mensagem,"USER_REMOVED");
            strcpy(message.motivo,"USER_REMOVED");
            pthread_mutex_lock(&mutex);
            fprintf(stderr, "[DEBUG] username=%s vai kick motivo = %s\n",usernamekick,message.motivo);
            int fff= open(usernamekick, O_WRONLY);
            write(fff, &message, sizeof(message));
            close(fff);
            pthread_mutex_unlock(&mutex);
        }else if (strncmp(buffer, "topics", 6) == 0) {
            printf("\nTopicos:\n");
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < num_topics; i++) {
                    printf(" Nome: %s\n", topics[i].nome);
                }
            pthread_mutex_unlock(&mutex);
        } else if (strncmp(buffer, "lock", 4) == 0) {
            char *topic_name = strtok(buffer + 5, "\n");
            if (topic_name) {
                if (blockTopic(topic_name, 1) > 0) {
                    printf("[INFO] Tópico %s bloqueado.\n", topic_name);
                } else {
                    printf("[INFO] Falha ao bloquear o tópico %s.\n", topic_name);
                }
            }
        }else if (strncmp(buffer, "users", 4) == 0) {
            printf("\nusers connected:\n");
            for(int conter;conter<10;conter++)
            {
                printf("\n%s",users[conter].username);
            }
        } else if (strncmp(buffer, "unlock", 6) == 0) {
            char *topic_name = strtok(buffer + 7, "\n");
            if (topic_name) {
                if (blockTopic(topic_name, 0) > 0) {
                    printf("[INFO] Tópico %s desbloqueado.\n", topic_name);
                } else {
                    printf("[INFO] Falha ao desbloquear o tópico %s.\n", topic_name);
                }
            }
        } else if (strcmp(buffer, "close\n") == 0) {
            printf("[INFO] Encerrando o servidor...\n");
            strcpy(message.mensagem,"END_SERVER");
            strcpy(message.motivo,"END_SERVER");
            pthread_mutex_lock(&mutex);
            fprintf(stderr, "[DEBUG] motivo do serividor fechar = %s\n", message.motivo);
            broadcastALL(message);
            pthread_mutex_unlock(&mutex);
            pthread_mutex_lock(&mutex);
            running = 0; // Sinaliza para encerrar
            pthread_mutex_unlock(&mutex);
        }
    }

    return NULL;
}

// Thread temporizadores e duracao
void *handleTimer(void *arg) {
    while (running) {
        sleep(5); 
    }

    return NULL;
}

int main() {
    char *msg_fich;
    if (createMsgFich(msg_fich) == 0) {
        printf("[ERRO] Erro ao criar armazenamento de texto\n");
        return -505;
    }

    // Cria o named FIFO de rececao do servidor
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

    pthread_join(client_thread, NULL);
    pthread_join(admin_thread, NULL);
    pthread_join(timer_thread, NULL);

    close(server_fd);
    unlink(SERVER_PIPE);
    printf("[INFO] Servidor encerrado.\n");

    return 0;
}
