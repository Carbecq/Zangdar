#ifndef LIBCHESS_POSITION_HPP
#define LIBCHESS_POSITION_HPP

#include "MoveList.h"
#include "Bitboard.h"
#include "types.h"
#include "defines.h"
#include "zobrist.h"
#include <ostream>
#include <string>
#include <vector>
#include "Move.h"
#include "evaluate.h"
#include "Attacks.h"
#include "NNUE.h"
#include "TranspositionTable.h"

// structure destinée à stocker l'historique de make_move.
// celle-ci sera nécessaire pour effectuer un unmake_move
struct Status
{
    U64  hash               = 0ULL;                     // nombre unique (?) correspondant à la position
    U64  pawn_hash          = 0ULL;                     // hash uniquement pour les pions
    MOVE move               = Move::MOVE_NONE;
    int  ep_square          = SquareType::NO_SQUARE;    // case en-passant : si les blancs jouent e2-e4, la case est e3
    U32  castling           = CastleType::CASTLE_NONE;  // droit au roque
    int  halfmove_counter   = 0;                        // nombre de demi-coups depuis la dernière capture ou le dernier mouvement de pion.
    int  fullmove_counter   = 1;                        // le nombre de coups complets. Il commence à 1 et est incrémenté de 1 après le coup des noirs.
    Bitboard checkers       = 0ULL;                     // bitboard des pièces ennemies me donnant échec
    Bitboard pinned         = 0ULL;                     // bitboard des pièces amies clouées
};

/*
     * The Halfmove Clock inside an chess position object takes care of enforcing the fifty-move rule.
     * This counter is reset after captures or pawn moves, and incremented otherwise.
     * Also moves which lose the castling rights, that is rook- and king moves from their initial squares,
     * including castling itself, increment the Halfmove Clock.
     * However, those moves are irreversible in the sense to reverse the same rights -
     * since once a castling right is lost, it is lost forever, as considered in detecting repetitions.
     *
     */


class Board
{
public:
    Board();
    Board(const std::string &fen) { set_fen(fen, false); }

    void clear() noexcept;

    void parse_position(std::istringstream &is);

    //! \brief  Retourne le camp à jouer
    [[nodiscard]] constexpr Color turn() const noexcept { return side_to_move; }

    //! \brief  Retourne le bitboard des pièces de la couleur indiquée
    template<Color C>
    [[nodiscard]] constexpr Bitboard occupancy_c() const noexcept { return colorPiecesBB[C]; }

    //! \brief  Retourne le bitboard des pièces du type indiqué
    template<PieceType P>
    [[nodiscard]] constexpr Bitboard occupancy_p() const noexcept { return typePiecesBB[P]; }

    //! \brief  Retourne le bitboard des pièces de la couleur indiquée
    //! et du type indiqué
    template<Color C, PieceType P>
    [[nodiscard]] constexpr Bitboard occupancy_cp() const noexcept { return colorPiecesBB[C] & typePiecesBB[P]; }

    //! \brief  Retourne le bitboard de toutes les pièces Blanches et Noires
    [[nodiscard]] constexpr Bitboard occupancy_all() const noexcept { return colorPiecesBB[WHITE] | colorPiecesBB[BLACK]; }

    //! \brief  Retourne le bitboard de toutes les cases vides
    [[nodiscard]] constexpr Bitboard occupancy_none() const noexcept { return ~occupancy_all(); }

    //! \brief  Retourne le bitboard des Fous et des Dames
    template<Color C> constexpr Bitboard diagonal_sliders() const { return occupancy_cp<C, BISHOP>() | occupancy_cp<C, QUEEN>(); }

    //! \brief  Retourne le bitboard des Tours et des Dames
    template<Color C> constexpr Bitboard orthogonal_sliders() const { return occupancy_cp<C, ROOK>() | occupancy_cp<C, QUEEN>(); }

    //! \brief Retourne le bitboard de toutes les pièces du camp "C" attaquant la case "sq"
    template <Color C> [[nodiscard]] constexpr Bitboard attackers(const int sq) const noexcept
    {
        // il faut regarder les attaques de pions depuis l'autre camp
        return( (Attacks::pawn_attacks<~C>(sq)         & occupancy_cp<C, PAWN>())                                           |
                (Attacks::knight_moves(sq)             & occupancy_cp<C, KNIGHT>())                                         |
                (Attacks::king_moves(sq)               & occupancy_cp<C, KING>())                                           |
                (Attacks::bishop_moves(sq, occupancy_all()) & (occupancy_cp<C, BISHOP>() | occupancy_cp<C, QUEEN>())) |
                (Attacks::rook_moves(sq,   occupancy_all()) & (occupancy_cp<C, ROOK>()   | occupancy_cp<C, QUEEN>())) );
    }

    //! \brief Retourne le bitboard de toutes les pièces du camp "C", sauf le roi, attaquant la case "sq"
    template <Color C> [[nodiscard]] constexpr Bitboard attackersButKing(const int sq) const noexcept
    {
        // il faut regarder les attaques de pions depuis l'autre camp
        return( (Attacks::pawn_attacks<~C>(sq)         & occupancy_cp<C, PAWN>())                                           |
                (Attacks::knight_moves(sq)             & occupancy_cp<C, KNIGHT>())                                         |
                (Attacks::bishop_moves(sq, occupancy_all()) & (occupancy_cp<C, BISHOP>() | occupancy_cp<C, QUEEN>())) |
                (Attacks::rook_moves(sq,   occupancy_all()) & (occupancy_cp<C, ROOK>()   | occupancy_cp<C, QUEEN>())) );
    }

    template <Color C>
    void calculate_checkers_pinned() noexcept;
    void calculate_hash(U64 &khash, U64 &phash) const;

    //! \brief  Retourne le Bitboard de TOUS les attaquants (Blancs et Noirs) de la case "sq"
    [[nodiscard]] Bitboard all_attackers(const int sq, const Bitboard occ) const noexcept
    {
        return( (Attacks::pawn_attacks(BLACK, sq) & occupancy_cp<WHITE, PAWN>())       |
                (Attacks::pawn_attacks(WHITE, sq) & occupancy_cp<BLACK, PAWN>())       |
                (Attacks::knight_moves(sq)        & typePiecesBB[KNIGHT])                         |
                (Attacks::king_moves(sq)          & typePiecesBB[KING])                           |
                (Attacks::bishop_moves(sq, occ)   & (typePiecesBB[BISHOP] | typePiecesBB[QUEEN])) |
                (Attacks::rook_moves(sq,   occ)   & (typePiecesBB[ROOK]   | typePiecesBB[QUEEN])) );
    }

    //! \brief Returns an attack bitboard where sliders are allowed
    //! to xray other sliders moving the same directions
    //  code venant de Weiss
    template<Color C> [[nodiscard]] Bitboard XRayBishopAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<QUEEN>() ^ occupancy_cp<C, BISHOP>();
        return(Attacks::bishop_moves(sq, occ));
    }
    template<Color C> [[nodiscard]] Bitboard XRayRookAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<QUEEN>() ^ occupancy_cp<C, ROOK>();
        return(Attacks::rook_moves(sq, occ));
    }
    template<Color C> [[nodiscard]] Bitboard XRayQueenAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<QUEEN>() ^ occupancy_cp<C, ROOK>() ^ occupancy_cp<C, BISHOP>();
        return(Attacks::queen_moves(sq, occ));
    }

    template <Color C> [[nodiscard]] Bitboard discoveredAttacks(const int sq) const noexcept;

    //        switch (pt)
    //        {
    //        case BISHOP:
    //            occ ^= pieces_cp<C, Queen>() ^ pieces_cp<C, BISHOP>();
    //            return(Attacks::bishop_moves(sq, occ));
    //            break;
    //        case ROOK:
    //            occ ^= pieces_cp<C, Queen>() ^ pieces_cp<C, ROOK>();
    //            return(Attacks::rook_moves(sq, occ));
    //            break;
    //        case Queen:
    //            occ ^= pieces_cp<C, Queen>() ^ pieces_cp<C, ROOK>() ^ pieces_cp<C, BISHOP>();
    //            return(Attacks::queen_moves(sq, occ));
    //            break;
    //        }
    //    }

    //! \brief  Détermine si case 'sq' est sur une colonne semi-ouverte
    //!  du point de vue 'C'
    template <Color C> [[nodiscard]] constexpr bool is_on_semiopen_file(int sq) const { return !(occupancy_cp<C, PAWN>() & FileMask64[sq]); }

    void set_fen(const std::string &fen, bool logTactics) noexcept;
    [[nodiscard]] std::string get_fen() const noexcept;
    void mirror_fen(const std::string &fen, bool logTactics);

    //! \brief  Retourne la position du roi
    template<Color C> [[nodiscard]] constexpr int king_square() const noexcept { return x_king[C]; }

    //! \brief Retourne le bitboard des cases attaquées
    template<Color C> [[nodiscard]] Bitboard squares_attacked() const noexcept;

    //! \brief  Détermine si la case sq est attaquée par le camp C
    template<Color C> [[nodiscard]] constexpr bool square_attacked(const int sq) const noexcept { return attackers<C>(sq) > 0; }

    //! \brief  Détermine si le camp au trait est en échec dans la position actuelle
    [[nodiscard]] inline bool is_in_check() const noexcept { return get_status().checkers > 0; }

    //! \brief  Détermine si le camp "C" attaque le roi ennemi
    template<Color C>
    [[nodiscard]] constexpr bool is_doing_check() const noexcept { return attackersButKing<C>(king_square<~C>()) > 0; }

    template<Color C, MoveGenType MGType> void legal_moves(MoveList &ml) noexcept;

    template<Color C> void apply_token(const std::string &token) noexcept;

    void verify_MvvLva();

    //==============================================
    //  Génération des coups

    void add_quiet_move(MoveList &ml, int from, int dest, PieceType piece, U32 flags) const noexcept;
    void add_capture_move(
        MoveList &ml,
        int from,
        int dest,
        PieceType piece,
        PieceType captured,
        U32 flags) const noexcept;
    void add_quiet_promotion(MoveList &ml, int from, int dest, PieceType promo) const noexcept;
    void add_capture_promotion(MoveList &ml,
                               int from,
                               int dest,
                               PieceType captured,
                               PieceType promo) const noexcept;

    void push_quiet_moves(MoveList &ml, Bitboard attack, const int from);
    void push_capture_moves(MoveList &ml, Bitboard attack, const int from);
    void push_piece_quiet_moves(MoveList &ml, Bitboard attack, const int from, PieceType piece);
    void push_piece_capture_moves(MoveList &ml, Bitboard attack, const int from, PieceType piece);
    void push_quiet_promotions(MoveList &ml, Bitboard attack, const int dir);
    void push_capture_promotions(MoveList &ml, Bitboard attack, const int dir);
    void push_quiet_promotion(MoveList &ml, const int from, const int to);
    void push_capture_promotion(MoveList &ml, const int from, const int to);
    void push_pawn_quiet_moves(MoveList &ml, Bitboard attack, const int dir, U32 flags);
    void push_pawn_capture_moves(MoveList &ml, Bitboard attack, const int dir);

    //------------------------------------------------------------
    //  le Roque

    template<Color C> constexpr bool can_castle() const
    {
        if constexpr (C == WHITE)
            return get_status().castling & (CASTLE_WK | CASTLE_WQ);
        else
            return get_status().castling & (CASTLE_BK | CASTLE_BQ);
    }

    template<Color C, CastleSide side> constexpr bool can_castle() const
    {
        if constexpr      (C == WHITE && side == KING_SIDE)
            return get_status().castling & CASTLE_WK;
        else if constexpr (C == WHITE && side == QUEEN_SIDE)
            return get_status().castling & CASTLE_WQ;
        else if constexpr (C == BLACK && side == KING_SIDE)
            return get_status().castling & CASTLE_BK;
        else if constexpr (C == BLACK && side == QUEEN_SIDE)
            return get_status().castling & CASTLE_BQ;
    }

    template <Color C, CastleSide side> constexpr Bitboard get_king_path()   // cases ne devant pas être attaquées
    {
        if constexpr      (C == WHITE && side == KING_SIDE)
            return F1G1_BB;
        else if constexpr (C == WHITE && side == QUEEN_SIDE)
            return C1D1_BB;
        else if constexpr (C == BLACK && side == KING_SIDE)
            return F8G8_BB;
        else if constexpr (C == BLACK && side == QUEEN_SIDE)
            return C8D8_BB;
    }

    template <Color C, CastleSide side> constexpr Bitboard get_rook_path()   // cases devant être libres
    {
        if constexpr      (C == WHITE && side == KING_SIDE)
            return F1G1_BB;
        else if constexpr (C == WHITE && side == QUEEN_SIDE)
            return B1D1_BB;
        else if constexpr (C == BLACK && side == KING_SIDE)
            return F8G8_BB;
        else if constexpr (C == BLACK && side == QUEEN_SIDE)
            return B8D8_BB;
    }

    template <Color C> constexpr int get_king_from()
    {
        if constexpr (C == WHITE)
            return (E1);
        else
            return (E8);
    }

    template <Color C, CastleSide side> constexpr int get_king_dest()
    {
        if constexpr      (C == WHITE && side == KING_SIDE)
            return (G1);
        else if constexpr (C == WHITE && side == QUEEN_SIDE)
            return (C1);
        else if constexpr (C == BLACK && side == KING_SIDE)
            return (G8);
        else if constexpr (C == BLACK && side == QUEEN_SIDE)
            return (C8);
    }

    /* Le roque nécessite plusieurs conditions :

    1) Toutes les cases qui séparent le roi de la tour doivent être vides.
        Par conséquent, il n'est pas permis de prendre une pièce adverse en roquant ;
    2) Ni le roi, ni la tour ne doivent avoir quitté leur position initiale.
        Par conséquent, le roi et la tour doivent être dans la première rangée du joueur,
        et chaque camp ne peut roquer qu'une seule fois par partie ;
        en revanche, le roi peut déjà avoir subi des échecs, s'il s'est soustrait à ceux-ci sans se déplacer.
    3) Aucune des cases (de départ, de passage ou d'arrivée) par lesquelles transite le roi lors du roque
        ne doit être contrôlée par une pièce adverse.
        Par conséquent, le roque n'est pas possible si le roi est en échec.
    4) En revanche la tour, ainsi que sa case adjacente dans le cas du grand roque, peuvent être menacées.


    A B C D E F G H
    T       R     T
    T         T R
        R T

    */

    //=======================================================================
    //! \brief  Ajoute les coups du roque
    //-----------------------------------------------------------------------
    template <Color C, CastleSide side> constexpr void gen_castle(MoveList& ml)
    {
        if (   can_castle<C, side>()
            && BB::empty(get_rook_path<C, side>() & occupancy_all())
            && !(squares_attacked<~C>() & get_king_path<C, side>()) )
        {
            add_quiet_move(ml, get_king_from<C>(), get_king_dest<C, side>(), KING, Move::FLAG_CASTLE_MASK);
        }
    }

    //=========================================================================

    template<Color C> void make_move(const MOVE move) noexcept;
    template<Color C> void undo_move() noexcept;
    template<Color C> void make_nullmove() noexcept;
    template<Color C> void undo_nullmove() noexcept;

    //! \brief  Retourne la couleur de la pièce située sur la case sq
    //! SUPPOSE qu'il y a une pièce sur cette case !!
    [[nodiscard]] constexpr Color color_on(const int sq) const noexcept
    {
        return( (colorPiecesBB[WHITE] & SQ::square_BB(sq)) ? WHITE : BLACK);
    }

    //! \brief  Retourne le type de la pièce située sur la case sq
    [[nodiscard]] constexpr PieceType piece_on(const int sq) const noexcept
    {
        for (int i = PAWN; i <= KING; ++i) {
            if (typePiecesBB[i] & SQ::square_BB(sq)) {
                return PieceType(i);
            }
        }
        return NO_TYPE;
    }


    bool valid() const noexcept;
    [[nodiscard]] std::string display() const noexcept;

    template<Color C> Bitboard getNonPawnMaterial() const noexcept
    {
        return (  colorPiecesBB[C]
                ^ occupancy_cp<C, PAWN>()
                ^ occupancy_cp<C, KING>() ) ;
    }

    template<Color C> int getNonPawnMaterialCount() const noexcept
    {
        return (BB::count_bit(colorPiecesBB[C]
                              ^ occupancy_cp<C, PAWN>()
                              ^ occupancy_cp<C, KING>() ) );
    }

    //==============================================
    //  Evaluation

    Score evaluate();
    Score evaluate_pieces(EvalInfo& ei);

    template <Color C> Score evaluate_pawns(EvalInfo& ei);
    template <Color C> Score evaluate_knights(EvalInfo& ei);
    template <Color C> Score evaluate_bishops(EvalInfo& ei);
    template <Color C> Score evaluate_rooks(EvalInfo& ei);
    template <Color C> Score evaluate_queens(EvalInfo& ei);
    template <Color C> Score evaluate_king(EvalInfo& ei);
    template <Color C> Score evaluate_threats(const EvalInfo& ei);
    template <Color C> Score evaluate_passed(EvalInfo& ei);
    template <Color C> Score evaluate_king_attacks(const EvalInfo& ei) const;

    int   scale_factor(const Score eval);
    void  init_eval_info(EvalInfo& ei);
    Score probe_pawn_cache(EvalInfo& ei);
    bool  fast_see(const MOVE move, const int threshold) const;

    //====================================================================
    //! \brief  Détermine s'il y a eu 50 coups sans prise ni coup de pion
    //--------------------------------------------------------------------
    [[nodiscard]] inline bool fiftymoves() const noexcept { return get_status().halfmove_counter >= 100; }

    //====================================================================
    //! \brief  Détermine s'il y a eu répétition de la même position
    //! Pour cela, on compare le hash code de la position.
    //! Voir Ethereal
    //! + lors du calcul de répétitions, il faut distinguer les posItions de recherche,
    //!   et les positions de la partie.
    //--------------------------------------------------------------------
    [[nodiscard]] inline bool is_repetition(int ply) const noexcept
    {
        int reps             = 0;
        U64 current_hash     = get_status().hash;
        int gamemove_counter = StatusHistory.size() - 1;
        int halfmove_counter = get_status().halfmove_counter;

        // Look through hash histories for our moves
        for (int i = gamemove_counter - 2; i >= 0; i -= 2)
        {
            // No draw can occur before a zeroing move
            if (i < gamemove_counter - halfmove_counter)
                break;

            // Check for matching hash with a two fold after the root,
            // or a three fold which occurs in part before the root move
            if (    StatusHistory[i].hash == current_hash
                && (   i > gamemove_counter - ply   // 2-fold : on considère des positions dans l'arbre de recherche
                    || ++reps == 2) )               // 3-fold : on considère toutes les positions de la partie
                return true;
        }

        return false;
    }

    //=============================================================================
    //! \brief  Détermine si la position est nulle
    //-----------------------------------------------------------------------------
    [[nodiscard]] inline bool is_draw(int ply) const noexcept { return ((is_repetition(ply) || fiftymoves())); }

    //=============================================================================
    //! \brief  Met une pièce à la case indiquée
    //-----------------------------------------------------------------------------
    void set_piece(const int square, const Color color, const PieceType piece) noexcept
    {
        colorPiecesBB[color] |= SQ::square_BB(square);
        typePiecesBB[piece]  |= SQ::square_BB(square);
        pieceOn[square]       = piece;

#if defined USE_NNUE
        nnue.update_feature<true>(color, piece, square);
#endif
    }

    bool test_mirror(const std::string &line);
    template<Color C, bool divide> [[nodiscard]] std::uint64_t perft(const int depth) noexcept;
    void test_value(const std::string& fen );


    //! \brief  Calcule la phase de la position sur 24 points.
    //! Cette phase dépend des pièces sur l'échiquier
    //! La phase va de 0 (EndGame) à 24 (MiddleGame), dans le cas où aucun pion n'a été promu.
    int  get_phase24();

    //! \brief Calcule la phase de la position sur 256 points.
    //! ceci pour avoir une meilleure granulométrie ?
    //! ouverture     : phase24 = 24 : phase256 = 256,5
    //! fin de partie :         =  0 :          = 0,5
    int get_phase256(int phase24) { return (phase24 * 256 + 12) / 24; }

    template <Color US> int get_material();

    //===========================================================================
    //  Syzygy

    void TBScore(const unsigned wdl, const unsigned dtz, int &score, int &bound) const;
    bool probe_wdl(int &score, int &bound, int ply) const;
    MOVE convertPyrrhicMove(unsigned result) const;
    bool probe_root(MOVE& move) const;

    //*************************************************************************
    //*************************************************************************
    //*************************************************************************

    //===========================================================================
    //  la position

    Bitboard colorPiecesBB[2] = {0ULL};     // bitboard des pièces pour chaque couleur
    Bitboard typePiecesBB[7]  = {0ULL};     // bitboard des pièces pour chaque type de pièce
    std::array<PieceType, 64> pieceOn;      // donne le type de la pièce occupant la case indiquée
    int x_king[2];                          // position des rois
    Color side_to_move = Color::WHITE;      // camp au trait
    std::vector<std::string> best_moves;    // meilleur coup (pour les tests tactiques)
    std::vector<std::string> avoid_moves;   // coup à éviter (pour les tests tactiques)

    //==============================================
    //  Status
    std::vector<Status> StatusHistory;      // historique de la partie (coups déjà joués ET coups de la recherche)
    inline const Status& get_status() const { return StatusHistory.back(); }
    inline Status& get_status()             { return StatusHistory.back(); }

    [[nodiscard]] inline int get_halfmove_counter() const noexcept { return get_status().halfmove_counter; }
    [[nodiscard]] inline int get_fullmove_counter() const noexcept { return get_status().fullmove_counter; }
    [[nodiscard]] inline int get_ep_square()        const noexcept { return get_status().ep_square; }
    [[nodiscard]] inline U64 get_hash()             const noexcept { return get_status().hash; }
    [[nodiscard]] inline U64 get_pawn_hash()        const noexcept { return get_status().pawn_hash; }
    [[nodiscard]] inline Bitboard get_checkers()    const noexcept { return get_status().checkers; }
    [[nodiscard]] inline Bitboard get_pinned()      const noexcept { return get_status().pinned; }

#if defined USE_NNUE
    NNUE nnue;
    inline void reserve_nnue_capacity() { nnue.reserve_nnue_capacity(); }  // la capacité ne passe pas avec la copie
#endif

};  // class Board





#endif
