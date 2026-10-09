#include "ui.h"
#include "db.h"

static GtkWidget *window;
static GtkWidget *stack;
static GtkWidget *lbl_status;

// Campos da tela de Login
static GtkWidget *entry_login_id, *entry_login_senha, *radio_login_ong;

// Campos da tela de Cadastro
static GtkWidget *entry_cad_nome, *entry_cad_id, *entry_cad_senha, *radio_cad_ong;

void ir_para_tela(const char *nome_tela) {
    gtk_label_set_text(GTK_LABEL(lbl_status), "");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), nome_tela);
}

static void on_btn_login_click(GtkWidget *widget, gpointer data) {
    const char *id_text = gtk_entry_get_text(GTK_ENTRY(entry_login_id));
    const char *senha_text = gtk_entry_get_text(GTK_ENTRY(entry_login_senha));
    gboolean is_ong = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_login_ong));

    if (autenticar_usuario(id_text, senha_text, is_ong)) {
        ir_para_tela(sessao.tipo == TIPO_ONG ? "painel_ong" : "painel_doador");
    } else {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Credenciais inválidas! Tente novamente.");
    }
}

static void on_btn_cadastrar_click(GtkWidget *widget, gpointer data) {
    const char *nome = gtk_entry_get_text(GTK_ENTRY(entry_cad_nome));
    const char *id_text = gtk_entry_get_text(GTK_ENTRY(entry_cad_id));
    const char *senha = gtk_entry_get_text(GTK_ENTRY(entry_cad_senha));
    gboolean is_ong = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_cad_ong));

    if (strlen(nome) == 0 || strlen(id_text) == 0 || strlen(senha) == 0) {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Preencha todos os campos!");
        return;
    }

    if (cadastrar_usuario(nome, id_text, senha, is_ong)) {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Cadastro realizado com sucesso! Faça login.");
        ir_para_tela("login");
    } else {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Erro ao cadastrar. Usuário/CNPJ já existe.");
    }
}

static void on_btn_logout_click(GtkWidget *widget, gpointer data) {
    encerrar_sessao();
    ir_para_tela("inicial");
}

static GtkWidget* criar_tela_inicial(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("<span size='x-large' weight='bold'>Bem-vindo ao Sistema!</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);

    GtkWidget *btn_login = gtk_button_new_with_label("Entrar na Conta");
    GtkWidget *btn_cadastro = gtk_button_new_with_label("Criar Nova Conta");

    g_signal_connect_swapped(btn_login, "clicked", G_CALLBACK(ir_para_tela), "login");
    
    // CORREÇÃO: Conecta o botão de cadastro para ir à tela de cadastro
    g_signal_connect_swapped(btn_cadastro, "clicked", G_CALLBACK(ir_para_tela), "cadastro");

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), btn_login, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), btn_cadastro, FALSE, FALSE, 5);

    return box;
}

static GtkWidget* criar_tela_login(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("<span size='large' weight='bold'>ENTRAR NO SISTEMA</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);

    radio_login_ong = gtk_radio_button_new_with_label(NULL, "É uma ONG");
    GtkWidget *radio_doador = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_login_ong), "É um Doador");

    GtkWidget *box_radio = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(box_radio), radio_login_ong, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_radio), radio_doador, TRUE, TRUE, 0);

    entry_login_id = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_login_id), "CNPJ / CPF / Email / Telefone");

    entry_login_senha = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_login_senha), "Digite sua senha");
    gtk_entry_set_visibility(GTK_ENTRY(entry_login_senha), FALSE);

    GtkWidget *btn_entrar = gtk_button_new_with_label("Acessar Conta");
    GtkWidget *btn_voltar = gtk_button_new_with_label("< Voltar");

    g_signal_connect(btn_entrar, "clicked", G_CALLBACK(on_btn_login_click), NULL);
    g_signal_connect_swapped(btn_voltar, "clicked", G_CALLBACK(ir_para_tela), "inicial");

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), box_radio, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), entry_login_id, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), entry_login_senha, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), btn_entrar, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), btn_voltar, FALSE, FALSE, 5);

    return box;
}

// NOVO: Função para criar a Tela de Cadastro de Usuário / ONG
static GtkWidget* criar_tela_cadastro(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("<span size='large' weight='bold'>CRIAR NOVA CONTA</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);

    radio_cad_ong = gtk_radio_button_new_with_label(NULL, "Cadastrar como ONG");
    GtkWidget *radio_cad_doador = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_cad_ong), "Cadastrar como Doador");

    GtkWidget *box_radio = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(box_radio), radio_cad_ong, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_radio), radio_cad_doador, TRUE, TRUE, 0);

    entry_cad_nome = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_cad_nome), "Nome completo ou Nome da ONG");

    entry_cad_id = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_cad_id), "CNPJ / CPF / Email / Telefone");

    entry_cad_senha = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_cad_senha), "Crie uma senha");
    gtk_entry_set_visibility(GTK_ENTRY(entry_cad_senha), FALSE);

    GtkWidget *btn_cadastrar = gtk_button_new_with_label("Confirmar Cadastro");
    GtkWidget *btn_voltar = gtk_button_new_with_label("< Voltar");

    g_signal_connect(btn_cadastrar, "clicked", G_CALLBACK(on_btn_cadastrar_click), NULL);
    g_signal_connect_swapped(btn_voltar, "clicked", G_CALLBACK(ir_para_tela), "inicial");

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), box_radio, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), entry_cad_nome, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), entry_cad_id, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), entry_cad_senha, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(box), btn_cadastrar, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), btn_voltar, FALSE, FALSE, 5);

    return box;
}

static GtkWidget* criar_painel_ong(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("<span size='large' weight='bold'>Painel da ONG</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);

    GtkWidget *btn_sair = gtk_button_new_with_label("Sair da Conta");
    g_signal_connect(btn_sair, "clicked", G_CALLBACK(on_btn_logout_click), NULL);

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), btn_sair, FALSE, FALSE, 10);

    return box;
}

static GtkWidget* criar_painel_doador(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("<span size='large' weight='bold'>Painel do Doador</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);

    GtkWidget *btn_sair = gtk_button_new_with_label("Sair da Conta");
    g_signal_connect(btn_sair, "clicked", G_CALLBACK(on_btn_logout_click), NULL);

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), btn_sair, FALSE, FALSE, 10);

    return box;
}

void inicializar_interface(GtkApplication *app) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Sistema de Doações - GTK");
    gtk_window_set_default_size(GTK_WINDOW(window), 850, 600);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    lbl_status = gtk_label_new("");

    stack = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(stack), criar_tela_inicial(), "inicial");
    gtk_stack_add_named(GTK_STACK(stack), criar_tela_login(), "login");
    gtk_stack_add_named(GTK_STACK(stack), criar_tela_cadastro(), "cadastro"); // REGISTRADO NO STACK
    gtk_stack_add_named(GTK_STACK(stack), criar_painel_ong(), "painel_ong");
    gtk_stack_add_named(GTK_STACK(stack), criar_painel_doador(), "painel_doador");

    gtk_box_pack_start(GTK_BOX(main_box), stack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), lbl_status, FALSE, FALSE, 10);

    gtk_container_add(GTK_CONTAINER(window), main_box);

    if (sessao.logado) {
        ir_para_tela(sessao.tipo == TIPO_ONG ? "painel_ong" : "painel_doador");
    } else {
        ir_para_tela("inicial");
    }

    gtk_widget_show_all(window);
}