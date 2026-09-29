#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SessaoAtiva sessao = { FALSE, 0, 0, "" };

void inicializar_banco(void) {
    sqlite3 *db;
    if (sqlite3_open("sistema_doacoes.db", &db) != SQLITE_OK) return;

    char *sql_ongs = "CREATE TABLE IF NOT EXISTS ONGs ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "razao_social TEXT, cnpj TEXT, categoria TEXT, email TEXT, telefone TEXT, "
                     "cep TEXT, endereco TEXT, descricao TEXT, chave_pix TEXT, banco TEXT, agencia TEXT, conta TEXT, "
                     "fotos TEXT, senha TEXT);";

    char *sql_doadores = "CREATE TABLE IF NOT EXISTS Doadores ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                         "nome TEXT, sobrenome TEXT, email TEXT, telefone TEXT, cpf TEXT, senha TEXT);";

    char *sql_doacoes = "CREATE TABLE IF NOT EXISTS Doacoes ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "id_ong INTEGER, id_doador INTEGER, anonimo INTEGER, metodo_pag INTEGER, valor REAL);";

    char *sql_sessao = "CREATE TABLE IF NOT EXISTS Sessao (id INTEGER PRIMARY KEY, tipo INTEGER, id_usuario INTEGER, nome TEXT);";

    sqlite3_exec(db, sql_ongs, 0, 0, 0);
    sqlite3_exec(db, sql_doadores, 0, 0, 0);
    sqlite3_exec(db, sql_doacoes, 0, 0, 0);
    sqlite3_exec(db, sql_sessao, 0, 0, 0);
    sqlite3_close(db);
}

void carregar_sessao(void) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    if (sqlite3_open("sistema_doacoes.db", &db) != SQLITE_OK) return;

    char *sql = "SELECT tipo, id_usuario, nome FROM Sessao WHERE id = 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            sessao.logado = TRUE;
            sessao.tipo = (TipoUsuario)sqlite3_column_int(stmt, 0);
            sessao.id_usuario = sqlite3_column_int(stmt, 1);
            const unsigned char *txt = sqlite3_column_text(stmt, 2);
            strcpy(sessao.nome_exibicao, txt ? (const char*)txt : "");
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void salvar_sessao(TipoUsuario tipo, int id, const char *nome) {
    sqlite3 *db;
    sqlite3_open("sistema_doacoes.db", &db);
    sqlite3_exec(db, "DELETE FROM Sessao;", 0, 0, 0);

    sqlite3_stmt *stmt;
    char *sql = "INSERT INTO Sessao (id, tipo, id_usuario, nome) VALUES (1, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, (int)tipo);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_bind_text(stmt, 3, nome, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    sessao.logado = TRUE;
    sessao.tipo = tipo;
    sessao.id_usuario = id;
    strcpy(sessao.nome_exibicao, nome);
}

void encerrar_sessao(void) {
    sqlite3 *db;
    sqlite3_open("sistema_doacoes.db", &db);
    sqlite3_exec(db, "DELETE FROM Sessao;", 0, 0, 0);
    sqlite3_close(db);
    sessao.logado = FALSE;
    sessao.id_usuario = 0;
    sessao.nome_exibicao[0] = '\0';
}

gboolean autenticar_usuario(const char *id_text, const char *senha_text, gboolean is_ong) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open("sistema_doacoes.db", &db);
    gboolean achou = FALSE;

    if (is_ong) {
        char *sql = "SELECT id, razao_social FROM ONGs WHERE (cnpj = ? OR email = ? OR telefone = ?) AND senha = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            for (int i = 1; i <= 3; i++) sqlite3_bind_text(stmt, i, id_text, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, senha_text, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                salvar_sessao(TIPO_ONG, sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1));
                achou = TRUE;
            }
        }
    } else {
        char *sql = "SELECT id, nome FROM Doadores WHERE (cpf = ? OR email = ? OR telefone = ?) AND senha = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            for (int i = 1; i <= 3; i++) sqlite3_bind_text(stmt, i, id_text, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, senha_text, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                salvar_sessao(TIPO_DOADOR, sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1));
                achou = TRUE;
            }
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return achou;
}