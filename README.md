# virtual-memory

Programa em C que implementa um gerenciador de memória virtual no estilo do projeto Designing a Virtual Memory Manager (Silberschatz et al., 10ª edição, p. P-51). A memória física tem 128 frames de 256 bytes, a tabela de páginas tem 256 entradas e a TLB tem 16 entradas. O programa recebe um arquivo de endereços lógicos e duas políticas de substituição, uma para página e outra para TLB, e grava a saída em correct.txt.

## Compilação

make

O Makefile gera o executável vm e compila com -Wall -Wextra -g -pthread.

## Execução

./vm address.txt lru fifo

O primeiro argumento é o arquivo de endereços lógicos. O segundo é a política de substituição de página (fifo ou lru). O terceiro é a política de substituição da TLB (fifo ou lru). O programa lê BACKING_STORE.bin do diretório atual e gera correct.txt.

## Limpeza

make clean

## Testes

make test ADDRESSES=/caminho/addresses.txt REFERENCE=/caminho/correct.txt

A suíte tests/run_tests.sh compila o programa, executa as quatro combinações de políticas, confere o formato da saída, compara o fluxo lru fifo com o gabarito e valida nove casos de erro. O resultado esperado é 31 passed, 0 failed.

## Estratégia do algoritmo

- **Endereço**: endereço lógico de 16 bits dividido em número de página (8 bits) e offset (8 bits).
- **Falta de página**: a página é carregada do BACKING_STORE.bin para um frame livre ou para o frame escolhido pela política de substituição.
- **Preenchimento**: os frames são preenchidos de 0 a 127; a substituição só começa quando a memória física está cheia.
- **Substituição de página**: fifo usa uma fila circular e lru usa o frame com menor contador de último uso. Ao remover uma página, a entrada correspondente na tabela é invalidada.
- **TLB**: 16 entradas com fifo ou lru. O lru preenche entradas inválidas antes de escolher uma vítima válida. A invalidação da página remove a entrada correspondente da TLB.
- **Busca na TLB**: cada acesso cria uma thread para cada entrada da TLB; cada thread verifica um slot e o resultado é reunido após o join.
- **Saída**: correct.txt segue o formato do gabarito, com endereço virtual, slot da TLB, endereço físico e valor, seguido das estatísticas de faltas e de acertos na TLB.
- **Validação**: número e tipo de argumentos, existência e formato do arquivo de endereços e faixa de 0 a 65535. Em qualquer erro o programa imprime uma mensagem em stderr e encerra com código de falha.

## Estrutura do código

- **vm.c**: leitura e validação dos argumentos e endereços, tabela de páginas, memória física, TLB, substituição, tradução e geração de correct.txt.
- **Makefile**: regras de compilação, execução, testes e limpeza.
- **tests/run_tests.sh**: suíte de integração com as quatro combinações de políticas e casos de erro.
- **evidencias.log**: sessões de terminal usadas como evidência.

## Ambiente

Implementado e validado em Linux (CachyOS) com gcc 16.2.1 e GNU make 4.4.1. O programa usa apenas a biblioteca padrão do C e pthreads.
