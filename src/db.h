#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <glib.h>

typedef enum { TIPO_ONG = 1, TIPO_DOADOR = 2 } TipoUsuario;

typedef struct {
    gboolean logado;
    TipoUsuario tipo;
    int id_usuario;
    char nome_exibicao[100];
} SessaoAtiva;

// Variável global de sessão acessível em todo o projeto
extern SessaoAtiva sessao;

void inicializar_banco(void);
void carregar_sessao(void);
void salvar_sessao(TipoUsuario tipo, int id, const char *nome);
void encerrar_sessao(void);

gboolean autenticar_usuario(const char *id_text, const char *senha_text, gboolean is_ong);

#endif