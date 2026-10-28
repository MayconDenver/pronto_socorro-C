/*
 * ============================================================================
 *  SISTEMA DE PRONTO-SOCORRO COM TRIAGEM MANCHESTER
 *  Projeto acadêmico de Estruturas de Dados em C
 * ============================================================================
 *
 *  Estruturas utilizadas:
 *    - LISTA ENCADEADA : cadastro de todos os pacientes (cada paciente existe
 *                        uma única vez na memória, como um nó desta lista).
 *    - 5 FILAS (FIFO)  : ordem de atendimento, uma fila por cor. As filas
 *                        guardam apenas PONTEIROS para os pacientes da lista.
 *    - PILHA (LIFO)    : prontuários dos atendimentos (o último registrado
 *                        é o primeiro consultado ou desfeito).
 *
 *  Compilar:   gcc -Wall -Wextra -std=c99 pronto_socorro.c -o pronto_socorro
 *  Executar:   ./pronto_socorro            (Windows: pronto_socorro.exe)
 *  Demo:       ./pronto_socorro --demo < demo.txt
 *
 *  AVISO: protótipo didático. Não deve ser usado para decisões reais de saúde.
 * ============================================================================
 */

/* Visual Studio: evita o erro C4996 em fopen/localtime ("função insegura"). */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>   /* SetConsoleOutputCP: acentos no terminal do Windows */
#endif

/* ----------------------------------------------------------------------------
 *  CONSTANTES
 * ------------------------------------------------------------------------- */

#define TAM_NOME 60
#define TAM_OBS 120
#define TAM_LINHA 256
#define ARQUIVO_PRONTUARIOS "prontuarios.txt"

typedef enum { VERMELHO, LARANJA, AMARELO, VERDE, AZUL, NUM_PRIORIDADES } Prioridade;
typedef enum { AGUARDANDO, ATENDIDO } Status;

static const char *NOME_COR[NUM_PRIORIDADES] = {
    "VERMELHO", "LARANJA", "AMARELO", "VERDE", "AZUL"
};
static const char *DESCRICAO_COR[NUM_PRIORIDADES] = {
    "Emergência", "Muito urgente", "Urgente", "Pouco urgente", "Não urgente"
};

/* Tempo máximo (em minutos) que um paciente pode ficar em cada cor.
 * Se atingir esse tempo, sobe UM nível. Vermelho já é o nível máximo.
 * Valores de referência do protocolo, usados aqui só para fins didáticos. */
static const int LIMITE_MINUTOS[NUM_PRIORIDADES] = { 0, 10, 60, 120, 240 };

/* ----------------------------------------------------------------------------
 *  ESTRUTURAS DE DADOS
 * ------------------------------------------------------------------------- */

/* Nó da LISTA ENCADEADA: guarda os dados do paciente. */
typedef struct Paciente {
    int id;
    char nome[TAM_NOME];
    Prioridade prioridade;
    Status status;
    time_t chegada;          /* hora do cadastro                       */
    time_t entrada_nivel;    /* hora em que entrou na cor atual        */
    struct Paciente *prox;   /* próximo paciente da lista              */
} Paciente;

typedef struct {
    Paciente *inicio;
    Paciente *fim;
    int tamanho;
    int proximo_id;
} ListaPacientes;

/* Nó da FILA: aponta para o paciente da lista (não copia os dados). */
typedef struct NoFila {
    Paciente *paciente;
    struct NoFila *prox;
} NoFila;

typedef struct {
    NoFila *inicio;          /* quem será atendido primeiro            */
    NoFila *fim;             /* onde entra quem acabou de chegar       */
    int tamanho;
} Fila;

/* Nó da PILHA: cópia dos dados no momento do atendimento (registro histórico). */
typedef struct Prontuario {
    int id_paciente;
    char nome[TAM_NOME];
    Prioridade prioridade;
    time_t hora_atendimento;
    int minutos_espera;
    char observacao[TAM_OBS];
    struct Prontuario *abaixo;   /* prontuário logo abaixo na pilha    */
} Prontuario;

typedef struct {
    Prontuario *topo;
    int tamanho;
} Pilha;

/* Agrupa todo o estado do sistema em um só lugar. */
typedef struct {
    ListaPacientes lista;
    Fila filas[NUM_PRIORIDADES];
    Pilha prontuarios;
    long deslocamento_seg;   /* relógio simulado (opção 9 do menu)     */
} ProntoSocorro;

/* Quando ligado (--demo), repete na tela o que foi lido do arquivo. */
static int eco_entrada = 0;

/* ----------------------------------------------------------------------------
 *  RELÓGIO
 *  Usa o relógio real do computador (time.h). O deslocamento permite
 *  "avançar o tempo" na apresentação sem esperar horas de verdade.
 * ------------------------------------------------------------------------- */

static time_t agora(const ProntoSocorro *ps)
{
    return time(NULL) + ps->deslocamento_seg;
}

static int minutos_desde(const ProntoSocorro *ps, time_t inicio)
{
    return (int)(difftime(agora(ps), inicio) / 60.0);
}

static void formatar_hora(time_t t, char *buf, size_t tam)
{
    struct tm *tm_info = localtime(&t);
    if (tm_info == NULL || strftime(buf, tam, "%H:%M", tm_info) == 0)
        snprintf(buf, tam, "--:--");
}

/* ----------------------------------------------------------------------------
 *  LEITURA SEGURA DE DADOS
 * ------------------------------------------------------------------------- */

/* Lê uma linha inteira (aceita espaços). Retorna 0 se a entrada acabou. */
static int ler_linha(const char *rotulo, char *buf, size_t tam)
{
    printf("%s", rotulo);
    fflush(stdout);

    if (fgets(buf, (int)tam, stdin) == NULL)
        return 0;

    if (strchr(buf, '\n') == NULL) {          /* linha maior que o buffer */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    buf[strcspn(buf, "\r\n")] = '\0';        /* remove a quebra de linha */

    if (eco_entrada)
        printf("%s\n", buf);
    return 1;
}

/* Lê um inteiro entre min e max, repetindo até ser válido. */
static int ler_inteiro(const char *rotulo, int min, int max, int *saida)
{
    char buf[TAM_LINHA];

    for (;;) {
        if (!ler_linha(rotulo, buf, sizeof buf))
            return 0;

        char *fim;
        long valor = strtol(buf, &fim, 10);
        while (*fim == ' ' || *fim == '\t')
            fim++;

        if (fim != buf && *fim == '\0' && valor >= min && valor <= max) {
            *saida = (int)valor;
            return 1;
        }
        printf("  Valor inválido. Digite um número entre %d e %d.\n", min, max);
    }
}

/* ----------------------------------------------------------------------------
 *  LISTA ENCADEADA  (cadastro)
 * ------------------------------------------------------------------------- */

static void lista_iniciar(ListaPacientes *l)
{
    l->inicio = NULL;
    l->fim = NULL;
    l->tamanho = 0;
    l->proximo_id = 1;
}

/* Insere no FIM da lista: O(1), graças ao ponteiro 'fim'. */
static Paciente *lista_inserir(ProntoSocorro *ps, const char *nome, Prioridade p)
{
    Paciente *novo = malloc(sizeof *novo);
    if (novo == NULL)
        return NULL;

    novo->id = ps->lista.proximo_id++;
    snprintf(novo->nome, sizeof novo->nome, "%s", nome);
    novo->prioridade = p;
    novo->status = AGUARDANDO;
    novo->chegada = agora(ps);
    novo->entrada_nivel = novo->chegada;
    novo->prox = NULL;

    if (ps->lista.fim != NULL)
        ps->lista.fim->prox = novo;
    else
        ps->lista.inicio = novo;
    ps->lista.fim = novo;
    ps->lista.tamanho++;
    return novo;
}

/* Busca sequencial por ID: O(n). */
static Paciente *lista_buscar(const ListaPacientes *l, int id)
{
    for (Paciente *p = l->inicio; p != NULL; p = p->prox)
        if (p->id == id)
            return p;
    return NULL;
}

static void lista_liberar(ListaPacientes *l)
{
    Paciente *p = l->inicio;
    while (p != NULL) {
        Paciente *seguinte = p->prox;
        free(p);
        p = seguinte;
    }
    lista_iniciar(l);
}

/* ----------------------------------------------------------------------------
 *  FILA  (ordem de atendimento dentro de cada cor)
 * ------------------------------------------------------------------------- */

static void fila_iniciar(Fila *f)
{
    f->inicio = NULL;
    f->fim = NULL;
    f->tamanho = 0;
}

/* Liga um nó já existente ao fim da fila: O(1). */
static void fila_anexar_no(Fila *f, NoFila *no)
{
    no->prox = NULL;
    if (f->fim != NULL)
        f->fim->prox = no;
    else
        f->inicio = no;
    f->fim = no;
    f->tamanho++;
}

/* Enfileirar: cria um nó e coloca no FIM. */
static int fila_enfileirar(Fila *f, Paciente *p)
{
    NoFila *no = malloc(sizeof *no);
    if (no == NULL)
        return 0;
    no->paciente = p;
    fila_anexar_no(f, no);
    return 1;
}

/* Coloca no INÍCIO da fila (usado só para desfazer um atendimento). */
static int fila_inserir_inicio(Fila *f, Paciente *p)
{
    NoFila *no = malloc(sizeof *no);
    if (no == NULL)
        return 0;
    no->paciente = p;
    no->prox = f->inicio;
    f->inicio = no;
    if (f->fim == NULL)
        f->fim = no;
    f->tamanho++;
    return 1;
}

/* Desenfileirar: retira do INÍCIO: O(1). */
static Paciente *fila_desenfileirar(Fila *f)
{
    if (f->inicio == NULL)
        return NULL;

    NoFila *no = f->inicio;
    Paciente *p = no->paciente;

    f->inicio = no->prox;
    if (f->inicio == NULL)
        f->fim = NULL;
    f->tamanho--;

    free(no);
    return p;
}

static void fila_liberar(Fila *f)
{
    while (fila_desenfileirar(f) != NULL) { }
}

/* ----------------------------------------------------------------------------
 *  PILHA  (prontuários)
 * ------------------------------------------------------------------------- */

static void pilha_iniciar(Pilha *pl)
{
    pl->topo = NULL;
    pl->tamanho = 0;
}

/* Push: o novo prontuário vira o topo: O(1). */
static int pilha_empilhar(Pilha *pl, const Paciente *p, int espera,
                          const char *obs, time_t hora)
{
    Prontuario *novo = malloc(sizeof *novo);
    if (novo == NULL)
        return 0;

    novo->id_paciente = p->id;
    snprintf(novo->nome, sizeof novo->nome, "%s", p->nome);
    novo->prioridade = p->prioridade;
    novo->hora_atendimento = hora;
    novo->minutos_espera = espera;
    snprintf(novo->observacao, sizeof novo->observacao, "%s", obs);

    novo->abaixo = pl->topo;
    pl->topo = novo;
    pl->tamanho++;
    return 1;
}

/* Pop: remove o topo e devolve uma cópia em 'saida': O(1). */
static int pilha_desempilhar(Pilha *pl, Prontuario *saida)
{
    if (pl->topo == NULL)
        return 0;

    Prontuario *removido = pl->topo;
    *saida = *removido;
    pl->topo = removido->abaixo;
    pl->tamanho--;
    free(removido);
    return 1;
}

static void pilha_liberar(Pilha *pl)
{
    Prontuario lixo;
    while (pilha_desempilhar(pl, &lixo)) { }
}

/* ----------------------------------------------------------------------------
 *  REGRAS DO PRONTO-SOCORRO
 * ------------------------------------------------------------------------- */

static void ps_iniciar(ProntoSocorro *ps)
{
    lista_iniciar(&ps->lista);
    for (int c = 0; c < NUM_PRIORIDADES; c++)
        fila_iniciar(&ps->filas[c]);
    pilha_iniciar(&ps->prontuarios);
    ps->deslocamento_seg = 0;
}

static void ps_liberar(ProntoSocorro *ps)
{
    for (int c = 0; c < NUM_PRIORIDADES; c++)
        fila_liberar(&ps->filas[c]);
    pilha_liberar(&ps->prontuarios);
    lista_liberar(&ps->lista);
}

/* Elevação de prioridade por tempo de espera.
 * Percorre as filas de LARANJA até AZUL. Quem atingiu o limite da própria
 * cor sai da fila atual e vai para o FIM da fila um nível mais urgente.
 * O mesmo nó é reaproveitado (sem malloc/free). Retorna quantos subiram. */
static int atualizar_prioridades(ProntoSocorro *ps)
{
    int movidos = 0;

    for (int c = LARANJA; c < NUM_PRIORIDADES; c++) {
        Fila *f = &ps->filas[c];
        NoFila *anterior = NULL;
        NoFila *no = f->inicio;

        while (no != NULL) {
            NoFila *seguinte = no->prox;
            Paciente *p = no->paciente;

            if (minutos_desde(ps, p->entrada_nivel) >= LIMITE_MINUTOS[c]) {
                /* 1. Desliga o nó da fila atual */
                if (anterior != NULL)
                    anterior->prox = seguinte;
                else
                    f->inicio = seguinte;
                if (f->fim == no)
                    f->fim = anterior;
                f->tamanho--;

                /* 2. Atualiza o paciente (que está na lista) */
                p->prioridade = (Prioridade)(c - 1);
                p->entrada_nivel = agora(ps);

                /* 3. Liga o mesmo nó no fim da fila mais urgente */
                fila_anexar_no(&ps->filas[c - 1], no);

                printf("  >> [%d] %s passou do limite de %d min: %s -> %s\n",
                       p->id, p->nome, LIMITE_MINUTOS[c],
                       NOME_COR[c], NOME_COR[c - 1]);
                movidos++;
            } else {
                anterior = no;
            }
            no = seguinte;
        }
    }
    return movidos;
}

/* ----------------------------------------------------------------------------
 *  OPÇÕES DO MENU
 * ------------------------------------------------------------------------- */

static void cadastrar_paciente(ProntoSocorro *ps)
{
    char nome[TAM_NOME];
    int opcao;

    printf("\n--- CADASTRAR PACIENTE ---\n");
    for (;;) {
        if (!ler_linha("Nome do paciente: ", nome, sizeof nome))
            return;
        if (nome[strspn(nome, " \t")] != '\0')
            break;
        printf("  O nome não pode ficar vazio.\n");
    }

    printf("Classificação de risco:\n");
    for (int c = 0; c < NUM_PRIORIDADES; c++)
        printf("  %d - %-8s (%s)\n", c + 1, NOME_COR[c], DESCRICAO_COR[c]);
    if (!ler_inteiro("Prioridade (1-5): ", 1, NUM_PRIORIDADES, &opcao))
        return;

    Paciente *p = lista_inserir(ps, nome, (Prioridade)(opcao - 1));
    if (p == NULL || !fila_enfileirar(&ps->filas[p->prioridade], p)) {
        printf("  Erro: memória insuficiente.\n");
        return;
    }
    printf("  Paciente cadastrado com ID %d na fila %s.\n",
           p->id, NOME_COR[p->prioridade]);
}

static void listar_pacientes(const ProntoSocorro *ps)
{
    printf("\n--- PACIENTES CADASTRADOS (%d) ---\n", ps->lista.tamanho);
    if (ps->lista.inicio == NULL) {
        printf("  Nenhum paciente cadastrado.\n");
        return;
    }

    printf("  %-4s %-22s %-9s %-11s %s\n", "ID", "NOME", "COR", "STATUS", "CHEGADA");
    for (const Paciente *p = ps->lista.inicio; p != NULL; p = p->prox) {
        char hora[8];
        formatar_hora(p->chegada, hora, sizeof hora);
        printf("  %-4d %-22.22s %-9s %-11s %s\n", p->id, p->nome,
               NOME_COR[p->prioridade],
               p->status == AGUARDANDO ? "Aguardando" : "Atendido", hora);
    }
}

static void mostrar_filas(const ProntoSocorro *ps)
{
    printf("\n--- FILAS DE ATENDIMENTO ---\n");
    for (int c = 0; c < NUM_PRIORIDADES; c++) {
        printf("  %-8s (%d): ", NOME_COR[c], ps->filas[c].tamanho);
        if (ps->filas[c].inicio == NULL)
            printf("vazia");
        for (const NoFila *no = ps->filas[c].inicio; no != NULL; no = no->prox) {
            const Paciente *p = no->paciente;
            printf("[%d] %s (%d min)%s", p->id, p->nome,
                   minutos_desde(ps, p->chegada), no->prox ? "  ->  " : "");
        }
        printf("\n");
    }
}

static void buscar_paciente(const ProntoSocorro *ps)
{
    int id;
    printf("\n--- BUSCAR PACIENTE ---\n");
    if (!ler_inteiro("ID do paciente: ", 1, 1000000, &id))
        return;

    const Paciente *p = lista_buscar(&ps->lista, id);
    if (p == NULL) {
        printf("  Paciente %d não encontrado.\n", id);
        return;
    }
    char hora[8];
    formatar_hora(p->chegada, hora, sizeof hora);
    printf("  ID: %d | Nome: %s | Cor: %s (%s)\n", p->id, p->nome,
           NOME_COR[p->prioridade], DESCRICAO_COR[p->prioridade]);
    printf("  Status: %s | Chegada: %s | Espera total: %d min\n",
           p->status == AGUARDANDO ? "Aguardando" : "Atendido", hora,
           minutos_desde(ps, p->chegada));
}

static void atender_proximo(ProntoSocorro *ps)
{
    printf("\n--- ATENDER PRÓXIMO ---\n");
    atualizar_prioridades(ps);          /* antes de chamar, revisa os tempos */

    /* Verifica as filas da mais urgente (VERMELHO) para a menos urgente. */
    Paciente *p = NULL;
    for (int c = VERMELHO; c < NUM_PRIORIDADES && p == NULL; c++)
        p = fila_desenfileirar(&ps->filas[c]);

    if (p == NULL) {
        printf("  Nenhum paciente aguardando.\n");
        return;
    }

    int espera = minutos_desde(ps, p->chegada);
    printf("  Chamando: [%d] %s | %s | esperou %d min\n",
           p->id, p->nome, NOME_COR[p->prioridade], espera);

    char obs[TAM_OBS];
    if (!ler_linha("Observação do atendimento: ", obs, sizeof obs))
        obs[0] = '\0';

    p->status = ATENDIDO;
    if (!pilha_empilhar(&ps->prontuarios, p, espera, obs, agora(ps)))
        printf("  Erro: memória insuficiente para o prontuário.\n");
    else
        printf("  Prontuário registrado no topo da pilha.\n");
}

static void opcao_atualizar_prioridades(ProntoSocorro *ps)
{
    printf("\n--- ATUALIZAR PRIORIDADES POR TEMPO ---\n");
    printf("  Limites: LARANJA %d | AMARELO %d | VERDE %d | AZUL %d min\n",
           LIMITE_MINUTOS[LARANJA], LIMITE_MINUTOS[AMARELO],
           LIMITE_MINUTOS[VERDE], LIMITE_MINUTOS[AZUL]);
    if (atualizar_prioridades(ps) == 0)
        printf("  Nenhum paciente atingiu o limite da sua cor.\n");
}

static void consultar_ultimo_prontuario(const ProntoSocorro *ps)
{
    printf("\n--- ÚLTIMO PRONTUÁRIO (topo da pilha, %d no total) ---\n",
           ps->prontuarios.tamanho);
    const Prontuario *r = ps->prontuarios.topo;
    if (r == NULL) {
        printf("  Nenhum prontuário registrado.\n");
        return;
    }
    char hora[8];
    formatar_hora(r->hora_atendimento, hora, sizeof hora);
    printf("  Paciente: [%d] %s | Cor: %s\n", r->id_paciente, r->nome,
           NOME_COR[r->prioridade]);
    printf("  Atendido às %s após %d min de espera\n", hora, r->minutos_espera);
    printf("  Observação: %s\n", r->observacao[0] ? r->observacao : "(sem observação)");
}

/* Desfaz o último registro: o paciente volta ao INÍCIO da sua fila. */
static void desfazer_ultimo_prontuario(ProntoSocorro *ps)
{
    printf("\n--- DESFAZER ÚLTIMO PRONTUÁRIO ---\n");
    Prontuario removido;
    if (!pilha_desempilhar(&ps->prontuarios, &removido)) {
        printf("  Nenhum prontuário para desfazer.\n");
        return;
    }

    Paciente *p = lista_buscar(&ps->lista, removido.id_paciente);
    if (p != NULL && fila_inserir_inicio(&ps->filas[p->prioridade], p)) {
        p->status = AGUARDANDO;
        printf("  Prontuário de [%d] %s removido. Paciente voltou ao início da fila %s.\n",
               p->id, p->nome, NOME_COR[p->prioridade]);
    } else {
        printf("  Prontuário removido, mas o paciente não pôde voltar à fila.\n");
    }
}

static void simular_tempo(ProntoSocorro *ps)
{
    int minutos;
    printf("\n--- SIMULAR PASSAGEM DE TEMPO ---\n");
    if (!ler_inteiro("Avançar quantos minutos? ", 1, 1440, &minutos))
        return;
    ps->deslocamento_seg += (long)minutos * 60;
    printf("  Relógio avançado em %d min.\n", minutos);
}

/* Bônus: grava os prontuários em arquivo ao sair (do topo para a base). */
static void salvar_prontuarios(const ProntoSocorro *ps)
{
    if (ps->prontuarios.topo == NULL)
        return;

    FILE *arq = fopen(ARQUIVO_PRONTUARIOS, "w");
    if (arq == NULL) {
        printf("  Não foi possível criar %s.\n", ARQUIVO_PRONTUARIOS);
        return;
    }
    fprintf(arq, "PRONTUÁRIOS (do mais recente para o mais antigo)\n\n");
    for (const Prontuario *r = ps->prontuarios.topo; r != NULL; r = r->abaixo) {
        char hora[8];
        formatar_hora(r->hora_atendimento, hora, sizeof hora);
        fprintf(arq, "[%d] %s | %s | atendido às %s | espera %d min | %s\n",
                r->id_paciente, r->nome, NOME_COR[r->prioridade], hora,
                r->minutos_espera, r->observacao);
    }
    fclose(arq);
    printf("  %d prontuário(s) salvo(s) em %s.\n",
           ps->prontuarios.tamanho, ARQUIVO_PRONTUARIOS);
}

/* ----------------------------------------------------------------------------
 *  PROGRAMA PRINCIPAL
 * ------------------------------------------------------------------------- */

static void mostrar_menu(const ProntoSocorro *ps)
{
    int aguardando = 0;
    for (int c = 0; c < NUM_PRIORIDADES; c++)
        aguardando += ps->filas[c].tamanho;

    printf("\n==============================================\n");
    printf("  PRONTO-SOCORRO | aguardando: %d | prontuários: %d\n",
           aguardando, ps->prontuarios.tamanho);
    printf("==============================================\n");
    printf("  1 - Cadastrar paciente\n");
    printf("  2 - Listar pacientes\n");
    printf("  3 - Mostrar filas de atendimento\n");
    printf("  4 - Buscar paciente por ID\n");
    printf("  5 - Atender próximo\n");
    printf("  6 - Atualizar prioridades por tempo\n");
    printf("  7 - Consultar último prontuário\n");
    printf("  8 - Desfazer último prontuário\n");
    printf("  9 - Simular passagem de tempo\n");
    printf("  0 - Sair\n");
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc > 1 && strcmp(argv[1], "--demo") == 0)
        eco_entrada = 1;

    ProntoSocorro ps;
    ps_iniciar(&ps);

    printf("Sistema de Pronto-Socorro com Triagem Manchester\n");
    printf("(protótipo didático: não usar para decisões reais de saúde)\n");

    int opcao;
    do {
        mostrar_menu(&ps);
        if (!ler_inteiro("Escolha uma opção: ", 0, 9, &opcao))
            opcao = 0;                       /* fim da entrada: encerra */

        switch (opcao) {
        case 1: cadastrar_paciente(&ps);           break;
        case 2: listar_pacientes(&ps);             break;
        case 3: mostrar_filas(&ps);                break;
        case 4: buscar_paciente(&ps);              break;
        case 5: atender_proximo(&ps);              break;
        case 6: opcao_atualizar_prioridades(&ps);  break;
        case 7: consultar_ultimo_prontuario(&ps);  break;
        case 8: desfazer_ultimo_prontuario(&ps);   break;
        case 9: simular_tempo(&ps);                break;
        case 0: printf("\nEncerrando...\n");        break;
        }
    } while (opcao != 0);

    salvar_prontuarios(&ps);
    ps_liberar(&ps);
    printf("Memória liberada. Até logo!\n");
    return 0;
}
