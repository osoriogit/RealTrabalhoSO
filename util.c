#include "util.h"

Pedido* createPedido(const char* username, const char* acao, const char* topico, int duracao, const char* mensagem) {
    Pedido* newPedido = (Pedido*)malloc(sizeof(Pedido));
    if (newPedido == NULL) {
        perror("Failed to allocate memory for Pedido");
        return NULL;
    }

    strncpy(newPedido->username, username, MAX_USERNAME_SIZE);
    strncpy(newPedido->acao, acao, 15);
    strncpy(newPedido->topico, topico, MAX_TOPIC_NAME);
    newPedido->duracao = duracao;
    strncpy(newPedido->mensagem, mensagem, MAX_MESSAGE_BODY);

    return newPedido;
}

// Function to free a Pedido
void freePedido(Pedido* pedido) {
    if (pedido) {
        free(pedido);
    }
}

// Function to create a new Resposta
Resposta* createResposta(const char* motivo, const char* mensagem) {
    Resposta* newResposta = (Resposta*)malloc(sizeof(Resposta));
    if (newResposta == NULL) {
        perror("Failed to allocate memory for Resposta");
        return NULL;
    }

    strncpy(newResposta->motivo, motivo, MAX_MESSAGE_BODY);
    strncpy(newResposta->mensagem, mensagem, MAX_MESSAGE_BODY);

    return newResposta;
}

// Function to free a Resposta
void freeResposta(Resposta* resposta) {
    if (resposta) {
        free(resposta);
    }
}

void setRespostaMensagem(Resposta *resposta, const char *mensagem) {
    if (resposta == NULL || mensagem == NULL) {
        fprintf(stderr, "[ERROR] Resposta or mensagem is NULL\n");
        return;
    }
    strncpy(resposta->mensagem, mensagem, MAX_MESSAGE_BODY - 1);
    resposta->mensagem[MAX_MESSAGE_BODY - 1] = '\0'; // Ensure null termination
}

// Function to update the 'motivo' field of a Resposta
void setRespostaMotivo(Resposta *resposta, const char *motivo) {
    if (resposta == NULL || motivo == NULL) {
        fprintf(stderr, "[ERROR] Resposta or motivo is NULL\n");
        return;
    }
    strncpy(resposta->motivo, motivo, MAX_MESSAGE_BODY - 1);
    resposta->motivo[MAX_MESSAGE_BODY - 1] = '\0'; // Ensure null termination
}


// Function to create a new Topic
Topics* createTopic(const char* nome) {
    Topics* newTopic = (Topics*)malloc(sizeof(Topics));
    if (newTopic == NULL) {
        perror("Failed to allocate memory for Topics");
        return NULL;
    }

    strncpy(newTopic->nome, nome, MAX_TOPIC_NAME);
    newTopic->nPersistent = 0;
    newTopic->isBlocked = 0;

    for (int i = 0; i < MAX_PERSISTENT_MESSAGES; i++) {
        newTopic->persistentMsg[i][0] = '\0'; // Initialize all messages as empty
    }

    return newTopic;
}

// Function to free a Topic
void freeTopic(Topics* topic) {
    if (topic) {
        free(topic);
    }
}

// Function to add a persistent message to a Topic
int addPersistentMessage(Topics* topic, const char* mensagem) {
    if (topic->nPersistent >= MAX_PERSISTENT_MESSAGES) {
        fprintf(stderr, "Failed to add message: Maximum persistent messages reached\n");
        return -1;
    }

    strncpy(topic->persistentMsg[topic->nPersistent], mensagem, MAX_MESSAGE_BODY);
    topic->nPersistent++;

    return 0; // Success
}

// Function to create a new User
Users* createUser(const char* username) {
    Users* newUser = (Users*)malloc(sizeof(Users));
    if (newUser == NULL) {
        perror("Failed to allocate memory for Users");
        return NULL;
    }

    strncpy(newUser->username, username, MAX_USERNAME_SIZE);
    newUser->nSubs = 0;

    for (int i = 0; i < MAX_TOPICS; i++) {
        newUser->subs[i][0] = '\0'; // Initialize all subscriptions as empty
    }

    return newUser;
}

// Function to free a User
void freeUser(Users* user) {
    if (user) {
        free(user);
    }
}

// Function to add a subscription to a User
int addUserSubscription(Users* user, const char* topic) {
    if (user->nSubs >= MAX_TOPICS) {
        fprintf(stderr, "Failed to add subscription: Maximum topics reached\n");
        return -1;
    }

    strncpy(user->subs[user->nSubs], topic, MAX_TOPIC_NAME);
    user->nSubs++;

    return 0; // Success
}