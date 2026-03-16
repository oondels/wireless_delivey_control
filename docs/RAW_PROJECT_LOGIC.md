# Regras de Negócio e Lógica de Controle: Sistema ESP32 (Principal e Remote)
O sistema de controle do carrinho é composto por uma arquitetura Mestre-Escravo utilizando dois microcontroladores ESP32 comunicando-se via ESP-NOW. A prioridade absoluta do sistema é a segurança (Fail-Safe), garantindo o acionamento imediato do freio em caso de falhas ou comandos de emergência.

### 1.1. Módulo Principal (Painel Central / Recepção e Atuação)
- **Localização:** Painel fixo no estacionamento/depósito.
- **Função Lógica:** Atua como o cérebro físico do sistema. Lê as entradas físicas do painel central, escuta os comandos sem fio do módulo Remote, e aciona os relés (5V) correspondentes aos comandos de direção, velocidade e freio.
- **Hierarquia:** Possui prioridade de comando sobre o módulo Remote para operações de subida/descida, controle de velocidade e gestão de emergência.

### 1.2. Módulo Remote (Carrinho / Transmissão)
- **Localização:** Embarcado no carrinho de transporte.
- **Função Lógica:** Atua como uma interface de controle móvel. Lê os botões acionados pelo operador que acompanha o carrinho e transmite as instruções para o módulo Principal via ESP-NOW.

## 2. Regras de Operação e Controle

### 2.1. Controle de Velocidade

- O sistema possui 3 níveis de velocidade, selecionados por botões de pulso.
- **Memória de Estado:** Ao receber um pulso de seleção, o software armazena a velocidade atual na memória.
- **Atuação:** O ESP32 Principal aciona a GPIO correspondente a um relé. Este relé, por sua vez, comuta a tensão para um potenciômetro físico pré-ajustado (a definição real da velocidade ocorre no hardware externo, não via PWM no software).
- **Sinalização:** Sempre que uma velocidade é selecionada e o relé é atracado, um LED indicativo correspondente deve ser acionado no painel.

### 2.2. Acionamento do Motor (Regra "Homem-Morto")

- O motor só entra em operação se o botão de acionamento for **mantido pressionado**.
- **Soltura do Botão:** Ao soltar o botão de acionamento, o sistema corta o motor e aciona o sistema de trava (freio) imediatamente.

### 2.3. Sistema de Trava e Freio

- O controle lógico de segurança impede o funcionamento do motor caso o sistema esteja "travado".
- O bloqueio ocorre fisicamente através do acionamento/desacionamento de uma microchave conectada ao freio mecânico.
- Sempre que a trava lógica é ativada no software, os comandos de movimentação vindos do módulo Remote são sumariamente ignorados.

## 3. Protocolos de Segurança e Emergência (Fail-Safe)

### 3.1. Condições de Acionamento Automático do Freio

O sistema de trava/freio é ativado de forma automática e imediata nas seguintes condições:
1. **Perda de Comunicação:** Se o ESP32 Principal parar de receber o sinal de vida (*heartbeat*) do ESP32 Remote.
2. **Queda de Energia:** Se o módulo Remote for desligado ou ficar sem bateria.
3. **Botão de Emergência:** Acionamento de qualquer botão de emergência (seja no Painel Central ou no Carrinho). O acionamento da emergência sobrepõe e anula qualquer comando de aceleração que esteja sendo pressionado.

### 3.2. Resolução e Prioridade da Emergência

- Comandos de emergência têm prioridade máxima na execução do software.
- **Controle pelo Painel Central:** O painel central pode ativar ou desativar o estado de emergência da máquina. Se a emergência for ativada pelo painel central, o freio é acionado imediatamente, ignorando qualquer requisição do módulo Remote.
- **Sobrescrita de Emergência do Remote:** Se a emergência for acionada fisicamente no carrinho (módulo Remote), o Painel Central possui autoridade para desativar esse estado de emergência remotamente. 
- **Restrição de Rearme:** A desativação da emergência pelo Painel Central deve ser estritamente **manual** (requer ação deliberada do operador no painel). O sistema jamais pode rearmar ou desativar uma emergência de forma automática.