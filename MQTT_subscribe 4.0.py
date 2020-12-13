# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import sys
import pandas as pd
import numpy as np

#definicoes:
Broker = "test.mosquitto.org"
PortaBroker = 1883
KeepAliveBroker = 60
TopicoSubscribe = "MicroEEL/VSM"
matriz = []
estoque = []
processo = []

estoque_matriz = []
processo_matriz = []


# dica: troque o nome do topico por algo "unico",
# Dessa maneira, ninguem ira saber seu topico de
# subscribe e interferir em seus testes

# Callback - conexao ao broker realizada
def on_connect(client, userdata, flags, rc):
    print("[STATUS] Conectado ao Broker. Resultado de conexao: " + str(rc))

    # faz subscribe automatico no topico
    client.subscribe(TopicoSubscribe)


# Callback - mensagem recebida do broker
def on_message(client, userdata, msg):
    key = 0

    MensagemRecebida = str(msg.payload)

    print("[MSG RECEBIDA] Topico: " + msg.topic + " / Mensagem: " + MensagemRecebida)
    dado_recebido = [str(MensagemRecebida.split()[0].replace("b'", '')), 
                     int(MensagemRecebida.split()[1]),
                     int(MensagemRecebida.split()[2].replace("'", ''))]

    # Criação da matriz principal (recebe os dados)
    for c in range(0, len(matriz)):
        if matriz[c][0] == dado_recebido[0] and dado_recebido[1] == 1:
            del(matriz[c])

    for c in range(0, len(estoque)):
        if estoque[c][0] == dado_recebido[0] and dado_recebido[1] == 1:
            del(estoque[c])

    for c in range(0, len(processo)):
        if processo[c][0] == dado_recebido[0] and dado_recebido[1] == 1:
            del(processo[c])

    if not matriz and dado_recebido[1] == 1:
        matriz.append([dado_recebido[0], dado_recebido[2]])
    else:
        for i in matriz:
            if i[0] == dado_recebido[0]:
                key = 1
                try:
                    i[dado_recebido[1]] = dado_recebido[2]
                except:
                    i.append(0)
                    i[dado_recebido[1]] = dado_recebido[2]
        if key == 0 and dado_recebido[1] == 1:
            matriz.append([dado_recebido[0], dado_recebido[2]])

    print(matriz)

    # Criação da matriz estoque e processo

    if not estoque and not processo:
        estoque.append([matriz[0][0]])
        processo.append([matriz[0][0]])
    else:
        key = 0
        for a in range(0, len(matriz)):
            for b in range(0, len(processo)):
                if processo[b][0] == matriz[a][0] and len(processo[b]) * 2 + 2 == len(matriz[a]):
                    if len(matriz[a]) % 2 == 0 and len(matriz[a]) > 2:
                        processo[b].append(matriz[a][len(matriz[a]) - 1] - matriz[a][len(matriz[a]) - 2])
                        key = 1
            for b in range(0, len(estoque)):
                if estoque[b][0] == matriz[a][0] and len(estoque[b]) * 2 + 1 == len(matriz[a]):
                    if len(matriz[a]) % 2 == 1 and len(matriz[a]) > 2:
                        estoque[b].append(matriz[a][len(matriz[a]) - 1] - matriz[a][len(matriz[a]) - 2])
                        key = 1
        if key == 0:
            estoque.append([matriz[len(matriz) - 1][0]])
            processo.append([matriz[len(matriz) - 1][0]])

    for i in matriz:
        if i[0] == dado_recebido[0] and dado_recebido[1] > 1:
            if len(i) % 2 == 1 and len(i) > 2:
                for n in estoque:
                    if n[0] == dado_recebido[0]:
                        x = [[n[0], len(n) - 1, n[-1], dado_recebido[2]]]
                        if x not in estoque_matriz and type(x[0][2]) != str:
                            estoque_matriz.append(x)
                            df_x = pd.DataFrame(x)
                            print(estoque)
                            with open('c:\projetoMicro\estoque.csv', 'a') as f:
                                df_x.to_csv(f, header=False, index=False)

            elif len(i) % 2 == 0 and len(i) > 2 and dado_recebido[1] > 2:
                for n in processo:
                    if n[0] == dado_recebido[0]:
                        y = [[n[0], len(n) - 1, n[-1], dado_recebido[2]]]
                        if y not in processo_matriz and type(y[0][2]) != str:
                            processo_matriz.append(y)
                            df_y = pd.DataFrame(y)
                            print(processo)
                            with open('c:\projetoMicro\processo.csv', 'a') as f:
                                df_y.to_csv(f, header=False, index=False)

    z = [dado_recebido]
    df_z = pd.DataFrame(z)
    with open('c:\projetoMicro\historico.csv', 'a') as f:
        df_z.to_csv(f, header=False, index=False)


# programa principal:
try:
    print("[STATUS] Inicializando MQTT...")
    # inicializa MQTT:
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(Broker, PortaBroker, KeepAliveBroker)
    client.loop_forever()
except KeyboardInterrupt:
    print("\nCtrl+C pressionado, encerrando aplicacao e saindo...")
    sys.exit(0)
