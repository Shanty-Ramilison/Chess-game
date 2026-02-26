#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define CELL   80          /* taille d'une case en pixels        */
#define BOARD  (CELL * 8)  /* largeur/hauteur du plateau          */
#define WIN_H  (BOARD + 50)/* hauteur totale (plateau + barre)   */
#define WIN_W  BOARD

/* Types de pièces */
#define EMPTY    0
#define PAWN     1
#define ROOK     2
#define KNIGHT   3
#define BISHOP   4
#define QUEEN    5
#define KING     6

/* Couleurs */
#define WHITE_C  1
#define BLACK_C  2

typedef struct {
    int type;    /* EMPTY, PAWN, ROOK, ... */
    int color;   /* WHITE_C ou BLACK_C      */
    int moved;   /* 1 si la pièce a bougé   */
} Piece;

typedef struct {
    int row, col;
} Pos;


static Piece board[8][8];
static int turn;          
static int sel_active;     
static Pos  sel;           


static int get_moves(Piece b[8][8], int r, int c, Pos moves[64])
{
    Piece *p = &b[r][c];
    int n = 0;


#define ADD(tr, tc) \
    do { \
        int _r=(tr), _c=(tc); \
        if (_r>=0&&_r<8&&_c>=0&&_c<8) { \
            if (b[_r][_c].type==EMPTY || b[_r][_c].color!=p->color) { \
                moves[n].row=_r; moves[n].col=_c; n++; \
            } \
        } \
    } while(0)


#define SLIDE(dr, dc) \
    do { \
        for (int i=1;i<8;i++) { \
            int tr=r+(dr)*i, tc=c+(dc)*i; \
            if (tr<0||tr>=8||tc<0||tc>=8) break; \
            if (b[tr][tc].type==EMPTY) { moves[n].row=tr;moves[n].col=tc;n++; } \
            else { if(b[tr][tc].color!=p->color){moves[n].row=tr;moves[n].col=tc;n++;} break; } \
        } \
    } while(0)

    if (p->type == PAWN) {
        int dir = (p->color == WHITE_C) ? -1 : 1;
        /* avancer */
        if (r+dir>=0 && r+dir<8 && b[r+dir][c].type==EMPTY) {
            moves[n].row=r+dir; moves[n].col=c; n++;
            /* double premier mouvement */
            if (!p->moved && b[r+2*dir][c].type==EMPTY) {
                moves[n].row=r+2*dir; moves[n].col=c; n++;
            }
        }
        /* captures diagonales */
        for (int dc=-1; dc<=1; dc+=2) {
            int tr=r+dir, tc=c+dc;
            if (tr>=0&&tr<8&&tc>=0&&tc<8)
                if (b[tr][tc].type!=EMPTY && b[tr][tc].color!=p->color) {
                    moves[n].row=tr; moves[n].col=tc; n++;
                }
        }
    }
    else if (p->type == ROOK) {
        SLIDE( 0, 1); SLIDE( 0,-1); SLIDE( 1, 0); SLIDE(-1, 0);
    }
    else if (p->type == KNIGHT) {
        int jumps[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (int i=0;i<8;i++) ADD(r+jumps[i][0], c+jumps[i][1]);
    }
    else if (p->type == BISHOP) {
        SLIDE( 1, 1); SLIDE( 1,-1); SLIDE(-1, 1); SLIDE(-1,-1);
    }
    else if (p->type == QUEEN) {
        SLIDE( 0, 1); SLIDE( 0,-1); SLIDE( 1, 0); SLIDE(-1, 0);
        SLIDE( 1, 1); SLIDE( 1,-1); SLIDE(-1, 1); SLIDE(-1,-1);
    }
    else if (p->type == KING) {
        int dirs[8][2]={{0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}};
        for (int i=0;i<8;i++) ADD(r+dirs[i][0], c+dirs[i][1]);

        /* Roque */
        if (!p->moved) {
            /* Petit roque */
            if (b[r][7].type==ROOK && !b[r][7].moved &&
                b[r][5].type==EMPTY && b[r][6].type==EMPTY) {
                moves[n].row=r; moves[n].col=6; n++;
            }
            /* Grand roque */
            if (b[r][0].type==ROOK && !b[r][0].moved &&
                b[r][1].type==EMPTY && b[r][2].type==EMPTY && b[r][3].type==EMPTY) {
                moves[n].row=r; moves[n].col=2; n++;
            }
        }
    }
#undef ADD
#undef SLIDE
    return n;
}

/* VÉRIFICATION : si case attaquée ?*/
static int is_attacked(Piece b[8][8], int r, int c, int by_color)
{
    Pos tmp[64];
    for (int row=0;row<8;row++)
        for (int col=0;col<8;col++)
            if (b[row][col].type!=EMPTY && b[row][col].color==by_color) {
                int n = get_moves(b, row, col, tmp);
                for (int i=0;i<n;i++)
                    if (tmp[i].row==r && tmp[i].col==c) return 1;
            }
    return 0;
}

/* ROI EN ÉCHEC ?*/
static int king_in_check(Piece b[8][8], int color)
{
    int kr=-1, kc=-1;
    for (int r=0;r<8;r++)
        for (int c=0;c<8;c++)
            if (b[r][c].type==KING && b[r][c].color==color) { kr=r; kc=c; }
    if (kr<0) return 0;
    int opp = (color==WHITE_C) ? BLACK_C : WHITE_C;
    return is_attacked(b, kr, kc, opp);
}

/*LE MOUVEMENT MET-IL LE ROI EN ÉCHEC ?*/
static int would_be_in_check(Piece b[8][8], int fr, int fc, int tr, int tc, int color)
{
    Piece tmp[8][8];
    memcpy(tmp, b, sizeof(tmp));
    tmp[tr][tc] = tmp[fr][fc];
    tmp[fr][fc].type = EMPTY;
    tmp[fr][fc].color = 0;
    return king_in_check(tmp, color);
}

/* ROQUE SÉCURISÉ ? (le roi ne passe pas par une case attaquée)*/
static int castling_safe(Piece b[8][8], int r, int to_col, int color)
{
    int opp = (color==WHITE_C) ? BLACK_C : WHITE_C;
    int cols[3];
    int start_col = 4; /* colonne initiale du roi */
    if (to_col == 6) { cols[0]=4; cols[1]=5; cols[2]=6; }
    else             { cols[0]=4; cols[1]=3; cols[2]=2; }
    for (int i=0;i<3;i++)
        if (is_attacked(b, r, cols[i], opp)) return 0;
    return 1;
}

/*Y A-T-IL AU MOINS UN COUP LÉGAL ?*/
static int has_legal_moves(Piece b[8][8], int color)
{
    Pos moves[64];
    for (int r=0;r<8;r++)
        for (int c=0;c<8;c++)
            if (b[r][c].type!=EMPTY && b[r][c].color==color) {
                int n = get_moves(b, r, c, moves);
                for (int i=0;i<n;i++) {
                    int tr=moves[i].row, tc=moves[i].col;
                    /* roque : vérification spéciale */
                    if (b[r][c].type==KING && abs(tc-c)==2) {
                        if (!king_in_check(b,color) && castling_safe(b,r,tc,color))
                            return 1;
                    } else {
                        if (!would_be_in_check(b,r,c,tr,tc,color))
                            return 1;
                    }
                }
            }
    return 0;
}

/* INITIALISATION DU PLATEAU*/
static void init_board(void)
{
    memset(board, 0, sizeof(board));

    int back[8] = {ROOK,KNIGHT,BISHOP,QUEEN,KING,BISHOP,KNIGHT,ROOK};

    for (int c=0;c<8;c++) {
        /* pions */
        board[1][c] = (Piece){PAWN, BLACK_C, 0};
        board[6][c] = (Piece){PAWN, WHITE_C, 0};
        /* pièces de fond */
        board[0][c] = (Piece){back[c], BLACK_C, 0};
        board[7][c] = (Piece){back[c], WHITE_C, 0};
    }
    turn = WHITE_C;
    sel_active = 0;
}

/* EXÉCUTER UN COUP (avec roque et promotion)
   Retourne 1 si coup valide*/
static int do_move(int fr, int fc, int tr, int tc)
{
    Piece *p = &board[fr][fc];
    if (p->type == EMPTY) return 0;

    /* Vérifier que (tr,tc) est dans les mouvements possibles */
    Pos moves[64];
    int n = get_moves(board, fr, fc, moves);
    int found = 0;
    for (int i=0;i<n;i++)
        if (moves[i].row==tr && moves[i].col==tc) { found=1; break; }
    if (!found) return 0;

    /* Roque */
    if (p->type==KING && abs(tc-fc)==2) {
        if (king_in_check(board, p->color)) return 0;
        if (!castling_safe(board, fr, tc, p->color)) return 0;

        Piece tmp[8][8];
        memcpy(tmp, board, sizeof(tmp));
        /* déplacer roi */
        tmp[fr][tc] = tmp[fr][fc];
        tmp[fr][fc].type = EMPTY;
        tmp[fr][tc].moved = 1;
        /* déplacer tour */
        if (tc==6) {
            tmp[fr][5] = tmp[fr][7];
            tmp[fr][7].type = EMPTY;
            tmp[fr][5].moved = 1;
        } else {
            tmp[fr][3] = tmp[fr][0];
            tmp[fr][0].type = EMPTY;
            tmp[fr][3].moved = 1;
        }
        memcpy(board, tmp, sizeof(board));
        return 1;
    }

    /* Coup normal : vérifier qu'on ne se met pas en échec */
    if (would_be_in_check(board, fr, fc, tr, tc, p->color)) return 0;

    board[tr][tc] = *p;
    board[fr][fc].type = EMPTY;
    board[fr][fc].color = 0;
    board[tr][tc].moved = 1;

    /* Promotion du pion */
    if (board[tr][tc].type==PAWN && (tr==0 || tr==7))
        board[tr][tc].type = QUEEN;

    return 1;
}


/* Symboles Unicode des pièces */
static const char *piece_symbol(int type, int color)
{
    /* Unicode chess symbols */
    if (color == WHITE_C) {
        switch(type) {
            case PAWN:   return "\xe2\x99\x99"; /* ♙ */
            case ROOK:   return "\xe2\x99\x96"; /* ♖ */
            case KNIGHT: return "\xe2\x99\x98"; /* ♘ */
            case BISHOP: return "\xe2\x99\x97"; /* ♗ */
            case QUEEN:  return "\xe2\x99\x95"; /* ♕ */
            case KING:   return "\xe2\x99\x94"; /* ♔ */
        }
    } else {
        switch(type) {
            case PAWN:   return "\xe2\x99\x9f"; /* ♟ */
            case ROOK:   return "\xe2\x99\x9c"; /* ♜ */
            case KNIGHT: return "\xe2\x99\x9e"; /* ♞ */
            case BISHOP: return "\xe2\x99\x9d"; /* ♝ */
            case QUEEN:  return "\xe2\x99\x9b"; /* ♛ */
            case KING:   return "\xe2\x99\x9a"; /* ♚ */
        }
    }
    return "";
}



static int get_legal_moves(Piece b[8][8], int r, int c, Pos out[64])
{
    Pos raw[64];
    int nraw  = get_moves(b, r, c, raw);
    int color = b[r][c].color;
    int n     = 0;

    for (int i = 0; i < nraw; i++) {
        int tr = raw[i].row, tc = raw[i].col;

        /* --- Cas special : roque (roi bouge de 2 colonnes) --- */
        if (b[r][c].type == KING && abs(tc - c) == 2) {
         
            if (!king_in_check(b, color) && castling_safe(b, r, tc, color)) {
                out[n].row = tr; out[n].col = tc; n++;
            }
        }
        /* --- Coup normal --- */
        else {
          
            if (!would_be_in_check(b, r, c, tr, tc, color)) {
                out[n].row = tr; out[n].col = tc; n++;
            }
        }
    }
    return n;
}

static void render(SDL_Renderer *ren, TTF_Font *font_piece, TTF_Font *font_ui)
{
    /* Couleurs des cases */
    SDL_Color light = {240, 217, 181, 255};
    SDL_Color dark  = { 181, 136, 99, 255};
    SDL_Color sel_color = {255, 255,   0, 255};
    SDL_Color hint_color = {100, 200, 100, 180};

    /* Fond blanc */
    SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
    SDL_RenderClear(ren);

    Pos hints[64];
    int nhints = 0;
    if (sel_active)
        nhints = get_legal_moves(board, sel.row, sel.col, hints);

    /* Dessiner les cases */
    for (int r=0;r<8;r++) {
        for (int c=0;c<8;c++) {
            SDL_Rect rect = {c*CELL, r*CELL, CELL, CELL};

            /* Couleur de base */
            SDL_Color col = ((r+c)%2==0) ? light : dark;

            /* Case sélectionnée */
            if (sel_active && sel.row==r && sel.col==c)
                col = sel_color;

            SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
            SDL_RenderFillRect(ren, &rect);

            /* Indicateurs de coup possible */
            if (sel_active) {
                for (int i=0;i<nhints;i++) {
                    if (hints[i].row==r && hints[i].col==c) {
                        /* Cercle simulé par un rectangle centré plus petit */
                        SDL_Rect dot = {c*CELL + CELL/3, r*CELL + CELL/3, CELL/3, CELL/3};
                        SDL_SetRenderDrawColor(ren, 100, 200, 100, 200);
                        SDL_RenderFillRect(ren, &dot);
                        break;
                    }
                }
            }

            /* Pièce */
            if (board[r][c].type != EMPTY) {
                const char *sym = piece_symbol(board[r][c].type, board[r][c].color);
                SDL_Color text_col = (board[r][c].color == WHITE_C)
                                     ? (SDL_Color){255,255,255,255}
                                     : (SDL_Color){20,20,20,255};
                SDL_Surface *surf = TTF_RenderUTF8_Blended(font_piece, sym, text_col);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
                    int tw = surf->w, th = surf->h;
                    SDL_Rect dst = {c*CELL + (CELL-tw)/2, r*CELL + (CELL-th)/2, tw, th};
                    SDL_RenderCopy(ren, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
            }
        }
    }

    /* Barre du bas : tour actuel + statut */
    {
        char msg[64];
        int in_check = king_in_check(board, turn);
        snprintf(msg, sizeof(msg), "Tour : %s%s",
                 (turn==WHITE_C) ? "Blancs" : "Noirs",
                 in_check ? "  — ECHEC !" : "");

        SDL_Color tc = {220,220,220,255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_ui, msg, tc);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_Rect dst = {10, BOARD + 10, surf->w, surf->h};
            SDL_RenderCopy(ren, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }

        /* Indication touche R */
        SDL_Color hint_tc = {150,150,150,255};
        SDL_Surface *surf2 = TTF_RenderUTF8_Blended(font_ui, "[R] Nouvelle partie", hint_tc);
        if (surf2) {
            SDL_Texture *tex2 = SDL_CreateTextureFromSurface(ren, surf2);
            SDL_Rect dst2 = {WIN_W - surf2->w - 10, BOARD + 10, surf2->w, surf2->h};
            SDL_RenderCopy(ren, tex2, NULL, &dst2);
            SDL_DestroyTexture(tex2);
            SDL_FreeSurface(surf2);
        }
    }

    SDL_RenderPresent(ren);
}

/* Boîte de message simple (titre + texte dans la console) */
static void show_message(SDL_Renderer *ren, TTF_Font *font, const char *title, const char *msg)
{
    /* On affiche dans le terminal et on attend 2 secondes */
    printf("\n*** %s : %s ***\n\n", title, msg);
    SDL_Delay(2000);
}


int main(void)
{
    /* ── Init SDL ── */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Echecs",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win); TTF_Quit(); SDL_Quit();
        return 1;
    }

    const char *font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
        NULL
    };

    TTF_Font *font_piece = NULL;
    TTF_Font *font_ui    = NULL;

    for (int i = 0; font_paths[i] && !font_piece; i++) {
        font_piece = TTF_OpenFont(font_paths[i], 52);
        if (font_piece) font_ui = TTF_OpenFont(font_paths[i], 20);
    }

    if (!font_piece) {
        fprintf(stderr,
            "Impossible de charger une police. Installez : "
            "sudo apt install fonts-dejavu\n%s\n", TTF_GetError());
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); TTF_Quit(); SDL_Quit();
        return 1;
    }

    /* ── Initialiser le jeu ── */
    init_board();

    /* ── Boucle principale ── */
    SDL_Event ev;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            }
            else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_r) {
                    /* Nouvelle partie */
                    init_board();
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                int col = ev.button.x / CELL;
                int row = ev.button.y / CELL;

                if (row < 0 || row >= 8 || col < 0 || col >= 8) break;

                if (sel_active) {
                    /* Tenter le déplacement */
                    if (do_move(sel.row, sel.col, row, col)) {
                        /* Changer de tour */
                        turn = (turn == WHITE_C) ? BLACK_C : WHITE_C;

                        /* Echec et mat ? */
                        if (!has_legal_moves(board, turn)) {
                            if (king_in_check(board, turn)) {
                                const char *winner = (turn == WHITE_C) ? "Noirs" : "Blancs";
                                char msg[64];
                                snprintf(msg, sizeof(msg), "%s gagnent la partie !", winner);
                                render(ren, font_piece, font_ui);
                                show_message(ren, font_ui, "Echec et Mat !", msg);
                            } else {
                                render(ren, font_piece, font_ui);
                                show_message(ren, font_ui, "Pat !", "Match nul - aucun coup legal");
                            }
                            init_board();
                        }
                    }
                    sel_active = 0;

                    /* Si on clique sur une autre pièce alliée, re-sélectionner */
                    if (board[row][col].type != EMPTY && board[row][col].color == turn) {
                        sel.row = row; sel.col = col;
                        sel_active = 1;
                    }
                }
                else {
                    /* Sélectionner une pièce alliée */
                    if (board[row][col].type != EMPTY && board[row][col].color == turn) {
                        sel.row = row; sel.col = col;
                        sel_active = 1;
                    }
                }
            }
        }

        render(ren, font_piece, font_ui);
        SDL_Delay(16); /* ~60 FPS */
    }

    /* ── Nettoyage ── */
    TTF_CloseFont(font_piece);
    TTF_CloseFont(font_ui);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
