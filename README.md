# Sistema de Pronto-Socorro com Triagem Manchester

Projeto acadêmico de Estruturas de Dados em C. O programa organiza pacientes de um pronto-socorro pela cor de triagem e registra os atendimentos em prontuários.

> Protótipo didático: não deve ser usado para decisões reais de saúde.

## Estruturas de dados

| Estrutura | Papel no sistema |
|---|---|
| **Lista encadeada** | Cadastro de todos os pacientes. Cada paciente existe uma única vez na memória. |
| **5 filas (FIFO)** | Ordem de atendimento, uma por cor. Guardam só **ponteiros** para os pacientes da lista. |
| **Pilha (LIFO)** | Prontuários dos atendimentos. O último registrado é o primeiro consultado ou desfeito. |

## Triagem e elevação de prioridade

| Cor | Nível | Limite na cor |
|---|---|---|
| VERMELHO | Emergência | imediato (nível máximo) |
| LARANJA | Muito urgente | 10 min |
| AMARELO | Urgente | 60 min |
| VERDE | Pouco urgente | 120 min |
| AZUL | Não urgente | 240 min |

O tempo vem do relógio real (`time.h`). Quem atinge o limite da sua cor sobe **um** nível e vai para o fim da fila mais urgente. A opção 9 do menu avança o relógio para a demonstração.

## Como compilar e executar

```bash
gcc -Wall -Wextra -std=c99 pronto_socorro.c -o pronto_socorro
./pronto_socorro                      # Windows: pronto_socorro.exe
./pronto_socorro --demo < demo.txt    # roteiro automático da apresentação
```

No Windows também funciona no Dev-C++ ou no Code::Blocks (abrir o `.c` e compilar).

## Menu

1. Cadastrar paciente
2. Listar pacientes
3. Mostrar filas de atendimento
4. Buscar paciente por ID
5. Atender próximo
6. Atualizar prioridades por tempo
7. Consultar último prontuário
8. Desfazer último prontuário
9. Simular passagem de tempo
0. Sair (salva os prontuários em `prontuarios.txt`)

## Complexidade

| Operação | Custo |
|---|---|
| Cadastrar, enfileirar, chamar próximo, registrar prontuário | O(1) |
| Buscar por ID, atualizar prioridades, desfazer prontuário | O(n) |

## Qualidade

- Compila sem warnings com `-Wall -Wextra -std=c99 -pedantic`.
- Valgrind: `All heap blocks were freed -- no leaks are possible`.

## Integrantes

- [Nome] — [parte do projeto]
- [Nome] — [parte do projeto]
