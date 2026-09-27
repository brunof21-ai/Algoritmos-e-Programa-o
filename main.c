#include "raylib.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Limites do sistema
#define MAX_ONGS 50
#define MAX_DOACOES 500
#define MAX_FOTOS 10

// Colunas da Matriz Bidimensional de Indicadores
#define COL_TIPO_DOADOR 0    // 1: PF, 2: PJ, 3: Anônimo
#define COL_METODO_PAG 1     // 1: Pix, 2: Cartão
#define COL_VALOR 2          // Valor em R$
#define COL_ID_ONG 3         // ID da ONG recebedora
#define TOTAL_COLUNAS 4

// Cores personalizadas para UI Moderna
#define COLOR_PRIMARY        (Color){ 41, 128, 185, 255 }  // Azul principal
#define COLOR_PRIMARY_DARK   (Color){ 31, 97, 141, 255 }   // Azul escuro cabeçalho
#define COLOR_ACCENT         (Color){ 39, 174, 96, 255 }   // Verde ação
#define COLOR_ACCENT_HOVER   (Color){ 46, 204, 113, 255 }  // Verde hover
#define COLOR_BG             (Color){ 245, 247, 250, 255 } // Fundo suave
#define COLOR_CARD           (Color){ 255, 255, 255, 255 } // Fundo dos cards
#define COLOR_TEXT_DARK      (Color){ 44, 62, 80, 255 }    // Texto principal
#define COLOR_TEXT_MUTED     (Color){ 127, 140, 141, 255 } // Texto secundário
#define COLOR_DANGER         (Color){ 231, 76, 60, 255 }   // Vermelho alertas/cancelar

typedef enum TelaSistema { 
    TELA_MENU, 
    TELA_CADASTRAR_ONG, 
    TELA_DOAR, 
    TELA_RELATORIOS 
} TelaSistema;

typedef struct {
    int id;
    char razao_social[100];
    char cnpj[20];
    char categoria[50];
    
    // Dados de Endereço
    char cep[15];
    char endereco[150];
    char descricao[500];
    
    // Dados Bancários
    char chave_pix[50];
    char banco[50];
    char agencia[20];
    char conta[20];

    // Fotos anexadas
    char fotos[MAX_FOTOS][256];
    int qtd_fotos;
} ONG;

ONG lista_ongs[MAX_ONGS];
int total_ongs = 0;

float matriz_indicadores[MAX_DOACOES][TOTAL_COLUNAS];
int total_doacoes = 0;

char msg_status[150] = "\0";

// --- FUNÇÕES DE BANCO DE DADOS (SQLite) ---

void inicializar_banco() {
    sqlite3 *db;
    char *err_msg = 0;
    
    int rc = sqlite3_open("sistema_doacoes.db", &db);
    if (rc != SQLITE_OK) {
        snprintf(msg_status, sizeof(msg_status), "Erro ao abrir BD: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    char *sql_ongs = "CREATE TABLE IF NOT EXISTS ONGs ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "razao_social TEXT, cnpj TEXT, categoria TEXT, "
                     "cep TEXT, endereco TEXT, descricao TEXT, "
                     "chave_pix TEXT, banco TEXT, agencia TEXT, conta TEXT, "
                     "fotos TEXT);";

    char *sql_doacoes = "CREATE TABLE IF NOT EXISTS Doacoes ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "id_ong INTEGER, tipo_doador INTEGER, metodo_pag INTEGER, valor REAL);";

    sqlite3_exec(db, sql_ongs, 0, 0, &err_msg);
    sqlite3_exec(db, sql_doacoes, 0, 0, &err_msg);
    sqlite3_close(db);
}

void carregar_dados_db() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    
    if (sqlite3_open("sistema_doacoes.db", &db) != SQLITE_OK) return;

    total_ongs = 0;
    char *sql_o = "SELECT id, razao_social, cnpj, categoria, cep, endereco, descricao, chave_pix, banco, agencia, conta FROM ONGs;";
    if (sqlite3_prepare_v2(db, sql_o, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && total_ongs < MAX_ONGS) {
            lista_ongs[total_ongs].id = sqlite3_column_int(stmt, 0);
            
            #define READ_STR(idx, target) \
                const unsigned char *tmp##idx = sqlite3_column_text(stmt, idx); \
                strcpy(target, tmp##idx ? (const char*)tmp##idx : "");

            READ_STR(1, lista_ongs[total_ongs].razao_social);
            READ_STR(2, lista_ongs[total_ongs].cnpj);
            READ_STR(3, lista_ongs[total_ongs].categoria);
            READ_STR(4, lista_ongs[total_ongs].cep);
            READ_STR(5, lista_ongs[total_ongs].endereco);
            READ_STR(6, lista_ongs[total_ongs].descricao);
            READ_STR(7, lista_ongs[total_ongs].chave_pix);
            READ_STR(8, lista_ongs[total_ongs].banco);
            READ_STR(9, lista_ongs[total_ongs].agencia);
            READ_STR(10, lista_ongs[total_ongs].conta);

            total_ongs++;
        }
    }
    sqlite3_finalize(stmt);

    total_doacoes = 0;
    char *sql_d = "SELECT tipo_doador, metodo_pag, valor, id_ong FROM Doacoes;";
    if (sqlite3_prepare_v2(db, sql_d, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && total_doacoes < MAX_DOACOES) {
            matriz_indicadores[total_doacoes][COL_TIPO_DOADOR] = (float)sqlite3_column_int(stmt, 0);
            matriz_indicadores[total_doacoes][COL_METODO_PAG]  = (float)sqlite3_column_int(stmt, 1);
            matriz_indicadores[total_doacoes][COL_VALOR]       = (float)sqlite3_column_double(stmt, 2);
            matriz_indicadores[total_doacoes][COL_ID_ONG]      = (float)sqlite3_column_int(stmt, 3);
            total_doacoes++;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

bool salvar_ong_db(ONG *ong) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open("sistema_doacoes.db", &db) != SQLITE_OK) return false;

    const char *sql = "INSERT INTO ONGs (razao_social, cnpj, categoria, cep, endereco, descricao, chave_pix, banco, agencia, conta, fotos) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        snprintf(msg_status, sizeof(msg_status), "Erro SQL: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, ong->razao_social, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ong->cnpj, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ong->categoria, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ong->cep, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, ong->endereco, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, ong->descricao, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, ong->chave_pix, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, ong->banco, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, ong->agencia, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, ong->conta, -1, SQLITE_TRANSIENT);

    char listaFotosStr[1024] = "";
    for (int i = 0; i < ong->qtd_fotos; i++) {
        strcat(listaFotosStr, ong->fotos[i]);
        if (i < ong->qtd_fotos - 1) strcat(listaFotosStr, ";");
    }
    sqlite3_bind_text(stmt, 11, listaFotosStr, -1, SQLITE_TRANSIENT);

    int step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (step == SQLITE_DONE) {
        carregar_dados_db();
        snprintf(msg_status, sizeof(msg_status), "ONG cadastrada com sucesso!");
        return true;
    }
    return false;
}

void salvar_doacao_db(int id_ong, int tipo_doador, int metodo_pag, float valor) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open("sistema_doacoes.db", &db);

    char *sql = "INSERT INTO Doacoes (id_ong, tipo_doador, metodo_pag, valor) VALUES (?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id_ong);
    sqlite3_bind_int(stmt, 2, tipo_doador);
    sqlite3_bind_int(stmt, 3, metodo_pag);
    sqlite3_bind_double(stmt, 4, valor);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    carregar_dados_db();
}

// --- INPUT CUSTOMIZADO ---
void InputBoxCustom(Rectangle box, char *text, int maxLen, int *letterCount, bool active, const char *placeholder) {
    Color corBorda = active ? COLOR_PRIMARY : (Color){ 210, 215, 220, 255 };
    Color corFundo = active ? WHITE : (Color){ 250, 252, 255, 255 };

    DrawRectangleRounded(box, 0.15f, 4, corFundo);
    DrawRectangleRoundedLinesEx(box, 0.15f, 4, 1.5f, corBorda);

    if (strlen(text) == 0 && !active) {
        DrawText(placeholder, (int)box.x + 10, (int)box.y + 8, 14, COLOR_TEXT_MUTED);
    } else {
        DrawText(text, (int)box.x + 10, (int)box.y + 8, 14, COLOR_TEXT_DARK);
    }

    if (active) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (*letterCount < maxLen)) {
                int codelength = 0;
                const char *utf8Char = CodepointToUTF8(key, &codelength);
                
                if (*letterCount + codelength <= maxLen) {
                    for (int i = 0; i < codelength; i++) {
                        text[*letterCount] = utf8Char[i];
                        (*letterCount)++;
                    }
                    text[*letterCount] = '\0';
                }
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && *letterCount > 0) {
            (*letterCount)--;
            while (*letterCount > 0 && (text[*letterCount] & 0xC0) == 0x80) {
                (*letterCount)--;
            }
            text[*letterCount] = '\0';
        }
    }
}

// --- COMPONENTE DE BOTÃO ESTILIZADO ---
bool DrawButton(Rectangle box, const char *text, Color baseColor, Color hoverColor) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, box);
    
    DrawRectangleRounded(box, 0.2f, 4, hover ? hoverColor : baseColor);
    
    int textSize = 16;
    int textWidth = MeasureText(text, textSize);
    int posX = (int)(box.x + (box.width - textWidth) / 2);
    int posY = (int)(box.y + (box.height - textSize) / 2);
    
    DrawText(text, posX, posY, textSize, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 650, "Sistema de Doações - ONGs");
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);

    inicializar_banco();
    carregar_dados_db();

    TelaSistema telaAtual = TELA_MENU;

    ONG novaOng = {0};
    int abaCadastro = 0;
    int campoAtivo = 0;
    
    int cRazao = 0, cCNPJ = 0, cCat = 0;
    int cCEP = 0, cEnd = 0, cDesc = 0;
    int cPix = 0, cBanco = 0, cAg = 0, cConta = 0;

    char inputValor[20] = "\0";
    int countV = 0;
    int ongSelecionadaIdx = 0;
    int tipoDoadorSel = 1; 
    int metodoPagSel = 1;  
    bool valorAtivo = false;

    while (!WindowShouldClose()) {
        int larg = GetScreenWidth();
        int alt = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

        if (IsFileDropped() && telaAtual == TELA_CADASTRAR_ONG && abaCadastro == 4) {
            FilePathList droppedFiles = LoadDroppedFiles();
            for (unsigned int i = 0; i < droppedFiles.count; i++) {
                if (novaOng.qtd_fotos < MAX_FOTOS) {
                    strncpy(novaOng.fotos[novaOng.qtd_fotos], droppedFiles.paths[i], 255);
                    novaOng.qtd_fotos++;
                }
            }
            UnloadDroppedFiles(droppedFiles);
        }

        BeginDrawing();
            ClearBackground(COLOR_BG);

            DrawRectangle(0, 0, larg, 60, COLOR_PRIMARY_DARK);
            DrawText("Sistema de Doações para ONGs", larg / 2 - MeasureText("Sistema de Doações para ONGs", 22) / 2, 18, 22, WHITE);

            if (strlen(msg_status) > 0) {
                DrawRectangle(larg / 2 - 300, alt - 40, 600, 30, (Color){ 231, 76, 60, 40 });
                DrawText(msg_status, larg / 2 - MeasureText(msg_status, 14) / 2, alt - 32, 14, COLOR_DANGER);
            }

            switch (telaAtual) {
                case TELA_MENU: {
                    Rectangle menuCard = { larg / 2 - 200, alt / 2 - 180, 400, 360 };
                    DrawRectangleRounded(menuCard, 0.05f, 4, COLOR_CARD);
                    DrawRectangleRoundedLinesEx(menuCard, 0.05f, 4, 1.0f, (Color){ 220, 224, 230, 255 });

                    DrawText("Painel Principal", larg / 2 - MeasureText("Painel Principal", 20) / 2, menuCard.y + 30, 20, COLOR_TEXT_DARK);
                    DrawText(TextFormat("ONGs Cadastradas: %d", total_ongs), larg / 2 - MeasureText(TextFormat("ONGs Cadastradas: %d", total_ongs), 16) / 2, menuCard.y + 65, 16, COLOR_TEXT_MUTED);

                    if (DrawButton((Rectangle){ menuCard.x + 50, menuCard.y + 110, 300, 45 }, "1. Cadastrar Nova ONG", COLOR_PRIMARY, (Color){ 52, 152, 219, 255 })) {
                        msg_status[0] = '\0';
                        memset(&novaOng, 0, sizeof(ONG));
                        cRazao = cCNPJ = cCat = cCEP = cEnd = cDesc = cPix = cBanco = cAg = cConta = 0;
                        abaCadastro = 0;
                        telaAtual = TELA_CADASTRAR_ONG;
                    }

                    if (DrawButton((Rectangle){ menuCard.x + 50, menuCard.y + 170, 300, 45 }, "2. Realizar Doação", COLOR_ACCENT, COLOR_ACCENT_HOVER)) {
                        msg_status[0] = '\0';
                        telaAtual = TELA_DOAR;
                    }

                    if (DrawButton((Rectangle){ menuCard.x + 50, menuCard.y + 230, 300, 45 }, "3. Relatórios / Indicadores", (Color){ 142, 68, 173, 255 }, (Color){ 155, 89, 182, 255 })) {
                        msg_status[0] = '\0';
                        telaAtual = TELA_RELATORIOS;
                    }
                    break;
                }

                case TELA_CADASTRAR_ONG: {
                    Rectangle card = { larg / 2 - 350, 80, 700, 500 };
                    DrawRectangleRounded(card, 0.03f, 4, COLOR_CARD);

                    DrawText("CADASTRO DE ONG (TODOS OS CAMPOS OBRIGATÓRIOS)", larg / 2 - MeasureText("CADASTRO DE ONG (TODOS OS CAMPOS OBRIGATÓRIOS)", 16) / 2, 95, 16, COLOR_PRIMARY_DARK);

                    const char *abas[] = { "1. Geral *", "2. Endereço *", "3. Detalhes *", "4. Banco *", "5. Fotos (mín. 2) *" };
                    for (int i = 0; i < 5; i++) {
                        Rectangle tabBox = { card.x + 15 + (i * 133), 130, 128, 35 };
                        Color colTab = (abaCadastro == i) ? COLOR_PRIMARY : (Color){ 230, 235, 240, 255 };
                        if (DrawButton(tabBox, abas[i], colTab, COLOR_PRIMARY)) {
                            abaCadastro = i;
                            campoAtivo = 0;
                        }
                    }

                    if (abaCadastro == 0) {
                        DrawText("Razão Social *:", card.x + 40, 190, 14, COLOR_TEXT_DARK);
                        Rectangle b1 = { card.x + 40, 210, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b1)) campoAtivo = 1;
                        InputBoxCustom(b1, novaOng.razao_social, 99, &cRazao, campoAtivo == 1, "Nome oficial da ONG");

                        DrawText("CNPJ *:", card.x + 40, 260, 14, COLOR_TEXT_DARK);
                        Rectangle b2 = { card.x + 40, 280, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b2)) campoAtivo = 2;
                        InputBoxCustom(b2, novaOng.cnpj, 19, &cCNPJ, campoAtivo == 2, "00.000.000/0001-00");

                        DrawText("Categoria *:", card.x + 40, 330, 14, COLOR_TEXT_DARK);
                        Rectangle b3 = { card.x + 40, 350, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b3)) campoAtivo = 3;
                        InputBoxCustom(b3, novaOng.categoria, 49, &cCat, campoAtivo == 3, "Ex: Educação, Meio Ambiente, Saúde...");
                    } 
                    else if (abaCadastro == 1) {
                        DrawText("CEP *:", card.x + 40, 190, 14, COLOR_TEXT_DARK);
                        Rectangle b1 = { card.x + 40, 210, 250, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b1)) campoAtivo = 1;
                        InputBoxCustom(b1, novaOng.cep, 14, &cCEP, campoAtivo == 1, "00000-000");

                        DrawText("Endereço Completo *:", card.x + 40, 260, 14, COLOR_TEXT_DARK);
                        Rectangle b2 = { card.x + 40, 280, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b2)) campoAtivo = 2;
                        InputBoxCustom(b2, novaOng.endereco, 149, &cEnd, campoAtivo == 2, "Rua, Número, Bairro, Cidade - UF");
                    }
                    else if (abaCadastro == 2) {
                        DrawText("Detalhes e História da ONG *:", card.x + 40, 190, 14, COLOR_TEXT_DARK);
                        Rectangle b1 = { card.x + 40, 210, 620, 150 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b1)) campoAtivo = 1;
                        InputBoxCustom(b1, novaOng.descricao, 499, &cDesc, campoAtivo == 1, "Descreva a missão, visão e objetivos...");
                    }
                    else if (abaCadastro == 3) {
                        DrawText("Chave Pix *:", card.x + 40, 190, 14, COLOR_TEXT_DARK);
                        Rectangle b1 = { card.x + 40, 210, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b1)) campoAtivo = 1;
                        InputBoxCustom(b1, novaOng.chave_pix, 49, &cPix, campoAtivo == 1, "E-mail, CPF/CNPJ ou Chave Aleatória");

                        DrawText("Banco *:", card.x + 40, 260, 14, COLOR_TEXT_DARK);
                        Rectangle b2 = { card.x + 40, 280, 620, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b2)) campoAtivo = 2;
                        InputBoxCustom(b2, novaOng.banco, 49, &cBanco, campoAtivo == 2, "Ex: Banco do Brasil, Itaú...");

                        DrawText("Agência *:", card.x + 40, 330, 14, COLOR_TEXT_DARK);
                        Rectangle b3 = { card.x + 40, 350, 290, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b3)) campoAtivo = 3;
                        InputBoxCustom(b3, novaOng.agencia, 19, &cAg, campoAtivo == 3, "0000-0");

                        DrawText("Conta Corrente *:", card.x + 370, 330, 14, COLOR_TEXT_DARK);
                        Rectangle b4 = { card.x + 370, 350, 290, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, b4)) campoAtivo = 4;
                        InputBoxCustom(b4, novaOng.conta, 19, &cConta, campoAtivo == 4, "00000-0");
                    }
                    else if (abaCadastro == 4) {
                        Rectangle dropZone = { card.x + 40, 190, 620, 100 };
                        DrawRectangleRounded(dropZone, 0.1f, 4, (Color){ 240, 244, 248, 255 });
                        DrawRectangleRoundedLinesEx(dropZone, 0.1f, 4, 1.5f, COLOR_PRIMARY);
                        DrawText("Arraste e solte fotos/logo da ONG aqui!", dropZone.x + dropZone.width/2 - MeasureText("Arraste e solte fotos/logo da ONG aqui!", 15)/2, dropZone.y + 40, 15, COLOR_PRIMARY);

                        DrawText(TextFormat("Fotos Anexadas: %d / %d (Mínimo de 2 fotos)", novaOng.qtd_fotos, MAX_FOTOS), card.x + 40, 310, 14, novaOng.qtd_fotos >= 2 ? COLOR_ACCENT : COLOR_DANGER);
                        for (int i = 0; i < novaOng.qtd_fotos; i++) {
                            DrawText(TextFormat("%d. %s", i + 1, GetFileName(novaOng.fotos[i])), card.x + 50, 335 + (i * 20), 13, COLOR_TEXT_MUTED);
                        }
                    }

                    // --- VALIDAÇÃO RIGOROSA ---
                    if (DrawButton((Rectangle){ card.x + 510, card.y + 435, 150, 45 }, "Salvar ONG", COLOR_ACCENT, COLOR_ACCENT_HOVER)) {
                        if (strlen(novaOng.razao_social) == 0 || strlen(novaOng.cnpj) == 0 || strlen(novaOng.categoria) == 0) {
                            snprintf(msg_status, sizeof(msg_status), "Preencha todos os campos da aba '1. Geral'!");
                            abaCadastro = 0;
                        } else if (strlen(novaOng.cep) == 0 || strlen(novaOng.endereco) == 0) {
                            snprintf(msg_status, sizeof(msg_status), "Preencha todos os campos da aba '2. Endereço'!");
                            abaCadastro = 1;
                        } else if (strlen(novaOng.descricao) == 0) {
                            snprintf(msg_status, sizeof(msg_status), "Preencha a descrição na aba '3. Detalhes'!");
                            abaCadastro = 2;
                        } else if (strlen(novaOng.chave_pix) == 0 || strlen(novaOng.banco) == 0 || strlen(novaOng.agencia) == 0 || strlen(novaOng.conta) == 0) {
                            snprintf(msg_status, sizeof(msg_status), "Preencha todos os dados bancários na aba '4. Banco'!");
                            abaCadastro = 3;
                        } else if (novaOng.qtd_fotos < 2) {
                            snprintf(msg_status, sizeof(msg_status), "É obrigatório anexar no mínimo 2 fotos na aba '5. Fotos'!");
                            abaCadastro = 4;
                        } else {
                            if (salvar_ong_db(&novaOng)) {
                                telaAtual = TELA_MENU;
                            }
                        }
                    }

                    if (DrawButton((Rectangle){ card.x + 40, card.y + 435, 130, 45 }, "< Voltar", COLOR_DANGER, (Color){ 236, 112, 99, 255 })) {
                        msg_status[0] = '\0';
                        telaAtual = TELA_MENU;
                    }
                    break;
                }

                case TELA_DOAR: {
                    Rectangle card = { larg / 2 - 300, 90, 600, 450 };
                    DrawRectangleRounded(card, 0.03f, 4, COLOR_CARD);

                    DrawText("REALIZAR DOAÇÃO", larg / 2 - MeasureText("REALIZAR DOAÇÃO", 20) / 2, 110, 20, COLOR_PRIMARY_DARK);

                    if (total_ongs == 0) {
                        DrawText("Nenhuma ONG cadastrada até o momento!", larg / 2 - MeasureText("Nenhuma ONG cadastrada até o momento!", 16) / 2, 240, 16, COLOR_DANGER);
                        if (DrawButton((Rectangle){ larg / 2 - 75, 320, 150, 45 }, "< Voltar", COLOR_DANGER, (Color){ 236, 112, 99, 255 })) telaAtual = TELA_MENU;
                    } else {
                        DrawText("Selecione a ONG de destino:", card.x + 40, 150, 14, COLOR_TEXT_DARK);
                        DrawRectangleRounded((Rectangle){ card.x + 40, 170, 460, 40 }, 0.15f, 4, (Color){ 240, 243, 246, 255 });
                        DrawText(lista_ongs[ongSelecionadaIdx].razao_social, card.x + 55, 182, 16, COLOR_PRIMARY_DARK);
                        
                        if (DrawButton((Rectangle){ card.x + 510, 170, 50, 40 }, ">", COLOR_PRIMARY, (Color){ 52, 152, 219, 255 })) {
                            ongSelecionadaIdx = (ongSelecionadaIdx + 1) % total_ongs;
                        }

                        DrawText("Tipo de Doador:", card.x + 40, 225, 14, COLOR_TEXT_DARK);
                        if (DrawButton((Rectangle){ card.x + 40, 245, 160, 35 }, "P. Física", tipoDoadorSel == 1 ? COLOR_PRIMARY : COLOR_TEXT_MUTED, COLOR_PRIMARY)) tipoDoadorSel = 1;
                        if (DrawButton((Rectangle){ card.x + 210, 245, 160, 35 }, "P. Jurídica", tipoDoadorSel == 2 ? COLOR_PRIMARY : COLOR_TEXT_MUTED, COLOR_PRIMARY)) tipoDoadorSel = 2;
                        if (DrawButton((Rectangle){ card.x + 380, 245, 140, 35 }, "Anônimo", tipoDoadorSel == 3 ? COLOR_PRIMARY : COLOR_TEXT_MUTED, COLOR_PRIMARY)) tipoDoadorSel = 3;

                        DrawText("Método de Pagamento:", card.x + 40, 295, 14, COLOR_TEXT_DARK);
                        if (DrawButton((Rectangle){ card.x + 40, 315, 230, 35 }, "Pix", metodoPagSel == 1 ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_ACCENT)) metodoPagSel = 1;
                        if (DrawButton((Rectangle){ card.x + 290, 315, 230, 35 }, "Cartão de Crédito", metodoPagSel == 2 ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_ACCENT)) metodoPagSel = 2;

                        DrawText("Valor da Doação (R$):", card.x + 40, 365, 14, COLOR_TEXT_DARK);
                        Rectangle bVal = { card.x + 40, 385, 250, 35 };
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, bVal)) valorAtivo = true;
                        InputBoxCustom(bVal, inputValor, 18, &countV, valorAtivo, "0.00");

                        if (DrawButton((Rectangle){ card.x + 350, 450, 210, 45 }, "Confirmar Doação", COLOR_ACCENT, COLOR_ACCENT_HOVER)) {
                            float val = atof(inputValor);
                            if (val > 0) {
                                salvar_doacao_db(lista_ongs[ongSelecionadaIdx].id, tipoDoadorSel, metodoPagSel, val);
                                inputValor[0] = '\0';
                                countV = 0;
                                snprintf(msg_status, sizeof(msg_status), "Doação de R$ %.2f registrada!", val);
                                telaAtual = TELA_MENU;
                            } else {
                                snprintf(msg_status, sizeof(msg_status), "Informe um valor válido maior que zero!");
                            }
                        }

                        if (DrawButton((Rectangle){ card.x + 40, 450, 130, 45 }, "< Voltar", COLOR_DANGER, (Color){ 236, 112, 99, 255 })) telaAtual = TELA_MENU;
                    }
                    break;
                }

                case TELA_RELATORIOS: {
                    DrawText("INDICADORES DE DOAÇÕES", larg / 2 - MeasureText("INDICADORES DE DOAÇÕES", 20) / 2, 85, 20, COLOR_PRIMARY_DARK);

                    if (total_doacoes == 0) {
                        DrawText("Nenhuma doação registrada no sistema ainda.", larg / 2 - MeasureText("Nenhuma doação registrada no sistema ainda.", 16) / 2, 250, 16, COLOR_TEXT_MUTED);
                    } else {
                        float total_arrecadado = 0;
                        int anonimos = 0, pix_count = 0;

                        for (int i = 0; i < total_doacoes; i++) {
                            total_arrecadado += matriz_indicadores[i][COL_VALOR];
                            if ((int)matriz_indicadores[i][COL_TIPO_DOADOR] == 3) anonimos++;
                            if ((int)matriz_indicadores[i][COL_METODO_PAG] == 1) pix_count++;
                        }

                        float media = total_arrecadado / total_doacoes;
                        float perc_anon = ((float)anonimos / total_doacoes) * 100.0f;
                        float perc_pix = ((float)pix_count / total_doacoes) * 100.0f;

                        int cWidth = 280;
                        int cGap = 20;
                        int startX = larg / 2 - cWidth - cGap / 2;

                        Rectangle c1 = { startX, 130, cWidth, 100 };
                        DrawRectangleRounded(c1, 0.08f, 4, COLOR_CARD);
                        DrawText("Total Arrecadado", c1.x + 20, c1.y + 15, 14, COLOR_TEXT_MUTED);
                        DrawText(TextFormat("R$ %.2f", total_arrecadado), c1.x + 20, c1.y + 45, 22, COLOR_PRIMARY_DARK);

                        Rectangle c2 = { startX + cWidth + cGap, 130, cWidth, 100 };
                        DrawRectangleRounded(c2, 0.08f, 4, COLOR_CARD);
                        DrawText("Média por Doação", c2.x + 20, c2.y + 15, 14, COLOR_TEXT_MUTED);
                        DrawText(TextFormat("R$ %.2f", media), c2.x + 20, c2.y + 45, 22, COLOR_ACCENT);

                        Rectangle c3 = { startX, 250, cWidth, 100 };
                        DrawRectangleRounded(c3, 0.08f, 4, COLOR_CARD);
                        DrawText("Doações Anônimas", c3.x + 20, c3.y + 15, 14, COLOR_TEXT_MUTED);
                        DrawText(TextFormat("%.1f%% (%d)", perc_anon, anonimos), c3.x + 20, c3.y + 45, 22, (Color){ 230, 126, 34, 255 });

                        Rectangle c4 = { startX + cWidth + cGap, 250, cWidth, 100 };
                        DrawRectangleRounded(c4, 0.08f, 4, COLOR_CARD);
                        DrawText("Pagamentos via Pix", c4.x + 20, c4.y + 15, 14, COLOR_TEXT_MUTED);
                        DrawText(TextFormat("%.1f%% (%d)", perc_pix, pix_count), c4.x + 20, c4.y + 45, 22, (Color){ 142, 68, 173, 255 });
                    }

                    if (DrawButton((Rectangle){ larg / 2 - 75, alt - 100, 150, 45 }, "< Voltar", COLOR_DANGER, (Color){ 236, 112, 99, 255 })) telaAtual = TELA_MENU;

                    break;
                }
            }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}