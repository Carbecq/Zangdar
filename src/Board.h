#ifndef LIBCHESS_POSITION_HPP
#define LIBCHESS_POSITION_HPP

#include "MoveList.h"
#include "Bitboard.h"
#include "types.h"
#include "defines.h"
#include <string>
#include <vector>
#include "Move.h"
#include "Attacks.h"
#include "NNUE.h"

// Structure définissant une position.
// Elle est destinée à stocker l'historique de make_move.
// celle-ci sera nécessaire pour effectuer un unmake_move
struct Status
{
    U64  key                = 0ULL;                     // nombre unique (?) correspondant à la position
    U64  pawn_key           = 0ULL;                     // nombre unique (?) correspondant à la position des pions
    U64  mat_key[2]         = {0ULL, 0ULL};             // nombre unique (?) correspondant à la position des pièces
    MOVE move               = Move::MOVE_NONE;
    int  ep_square          = SquareType::SQUARE_NONE;  // case en-passant : si les blancs jouent e2-e4, la case est e3
    U32  castling           = CASTLE_NONE;              // droit au roque
    int  fiftymove_counter  = 0;                        // nombre de demi-coups depuis la dernière capture ou le dernier mouvement de pion.
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
    Board(const std::string& fen);

    void reset() noexcept;
    void parse_position(std::istringstream &is);

    //! \brief  Retourne le camp à jouer
    [[nodiscard]] constexpr Color turn() const noexcept { return side_to_move; }

    //! \brief  Retourne le bitboard des pièces de la couleur indiquée
    template<Color C>
    [[nodiscard]] constexpr Bitboard occupancy_c() const noexcept { return colorPiecesBB[C]; }

    //! \brief  Retourne le bitboard des pièces du type indiqué
    template<PieceType P>
    [[nodiscard]] constexpr Bitboard occupancy_p() const noexcept { return typePiecesBB[static_cast<U32>(P)]; }

    //! \brief  Retourne le bitboard des pièces de la couleur indiquée
    //! et du type indiqué
    template<Color C, PieceType P>
    [[nodiscard]] constexpr Bitboard occupancy_cp() const noexcept { return colorPiecesBB[C] & typePiecesBB[static_cast<U32>(P)]; }

    //! \brief  Retourne le bitboard de toutes les pièces Blanches et Noires
    [[nodiscard]] constexpr Bitboard occupancy_all() const noexcept { return colorPiecesBB[WHITE] | colorPiecesBB[BLACK]; }

    //! \brief  Retourne le bitboard de toutes les cases vides
    [[nodiscard]] constexpr Bitboard occupancy_none() const noexcept { return ~occupancy_all(); }

    //! \brief  Retourne le bitboard des Fous et des Dames
    template<Color C> constexpr Bitboard diagonal_sliders() const { return occupancy_cp<C, PieceType::BISHOP>() | occupancy_cp<C, PieceType::QUEEN>(); }

    //! \brief  Retourne le bitboard des Tours et des Dames
    template<Color C> constexpr Bitboard orthogonal_sliders() const { return occupancy_cp<C, PieceType::ROOK>() | occupancy_cp<C, PieceType::QUEEN>(); }

    //! \brief Retourne le bitboard de toutes les pièces du camp "C" attaquant la case "sq"
    template <Color C> [[nodiscard]] constexpr Bitboard attackers(const int sq) const noexcept
    {
        // il faut regarder les attaques de pions depuis l'autre camp
        return( (Attacks::pawn_attacks<~C>(sq)         & occupancy_cp<C, PieceType::PAWN>())                                           |
                (Attacks::knight_moves(sq)             & occupancy_cp<C, PieceType::KNIGHT>())                                         |
                (Attacks::king_moves(sq)               & occupancy_cp<C, PieceType::KING>())                                           |
                (Attacks::bishop_moves(sq, occupancy_all()) & (occupancy_cp<C, PieceType::BISHOP>() | occupancy_cp<C, PieceType::QUEEN>())) |
                (Attacks::rook_moves(sq,   occupancy_all()) & (occupancy_cp<C, PieceType::ROOK>()   | occupancy_cp<C, PieceType::QUEEN>())) );
    }

    //! \brief Retourne le bitboard de toutes les pièces du camp "C", sauf le roi, attaquant la case "sq"
    template <Color C> [[nodiscard]] constexpr Bitboard attackersButKing(const int sq) const noexcept
    {
        // il faut regarder les attaques de pions depuis l'autre camp
        return( (Attacks::pawn_attacks<~C>(sq)         & occupancy_cp<C, PieceType::PAWN>())                                     |
                (Attacks::knight_moves(sq)             & occupancy_cp<C, PieceType::KNIGHT>())                                   |
                (Attacks::bishop_moves(sq, occupancy_all()) & (occupancy_cp<C, PieceType::BISHOP>() | occupancy_cp<C, PieceType::QUEEN>())) |
                (Attacks::rook_moves(sq,   occupancy_all()) & (occupancy_cp<C, PieceType::ROOK>()   | occupancy_cp<C, PieceType::QUEEN>())) );
    }

    //! \brief Retourne le bitboard de tous les pions du camp "C" attaquant la case "sq"
    template <Color C> [[nodiscard]] constexpr Bitboard pawn_attackers(const int sq) const noexcept
    {
        // il faut regarder les attaques de pions depuis l'autre camp
        return( Attacks::pawn_attacks<~C>(sq) & occupancy_cp<C, PieceType::PAWN>() );
    }


    template <Color C>
    void calculate_checkers_pinned() noexcept;
    void calculate_hash(U64& key, U64& pawn_key, U64 mat_key[2]) const;
    void calculate_nnue(int& eval) ;

    //! \brief  Retourne le Bitboard de TOUS les attaquants (Blancs et Noirs) de la case "sq"
    [[nodiscard]] Bitboard all_attackers(const int sq, const Bitboard occ) const noexcept
    {
        return( (Attacks::pawn_attacks(BLACK, sq) & occupancy_cp<WHITE, PieceType::PAWN>())             |
                (Attacks::pawn_attacks(WHITE, sq) & occupancy_cp<BLACK, PieceType::PAWN>())             |
                (Attacks::knight_moves(sq)        & typePiecesBB[static_cast<U32>(PieceType::KNIGHT )]) |
                (Attacks::king_moves(sq)          & typePiecesBB[static_cast<U32>(PieceType::KING   )]) |
                (Attacks::bishop_moves(sq, occ)   & (typePiecesBB[static_cast<U32>(PieceType::BISHOP)]  | typePiecesBB[static_cast<U32>(PieceType::QUEEN)])) |
                (Attacks::rook_moves(sq,   occ)   & (typePiecesBB[static_cast<U32>(PieceType::ROOK  )]  | typePiecesBB[static_cast<U32>(PieceType::QUEEN)])) );
    }

    //! \brief Returns an attack bitboard where sliders are allowed
    //! to xray other sliders moving the same directions
    //  code venant de Weiss
    template<Color C> [[nodiscard]] Bitboard XRayBishopAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<PieceType::QUEEN>() ^ occupancy_cp<C, PieceType::BISHOP>();
        return(Attacks::bishop_moves(sq, occ));
    }
    template<Color C> [[nodiscard]] Bitboard XRayRookAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<PieceType::QUEEN>() ^ occupancy_cp<C, PieceType::ROOK>();
        return(Attacks::rook_moves(sq, occ));
    }
    template<Color C> [[nodiscard]] Bitboard XRayQueenAttack(const int sq)
    {
        Bitboard occ = occupancy_all() ^ occupancy_p<PieceType::QUEEN>() ^ occupancy_cp<C, PieceType::ROOK>() ^ occupancy_cp<C, PieceType::BISHOP>();
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
    template <Color C> [[nodiscard]] constexpr bool is_on_semiopen_file(int sq) const { return !(occupancy_cp<C, PieceType::PAWN>() & FileMask64[sq]); }

    template <bool Update_NNUE> void set_fen(const std::string &fen, bool logTactics) noexcept;
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

    template<MoveGenType MGType> void legal_moves(MoveList &ml) noexcept
    {
        if (turn() == WHITE)
            legal_moves<WHITE, MGType>(ml);
        else
            legal_moves<BLACK, MGType>(ml);
    }
    template<Color C, MoveGenType MGType> void legal_moves(MoveList &ml) noexcept;

    template<Color C> void apply_token(const std::string &token) noexcept;

    void verify_MvvLva();

    //==============================================
    //  Génération des coups

    void add_quiet_move(  MoveList &ml, int from, int dest, Piece piece, U32 flags) const noexcept;
    void add_capture_move(MoveList &ml, int from, int dest, Piece piece, Piece captured, U32 flags) const noexcept;
    void add_quiet_promotion(  MoveList &ml, int from, int dest, Color color, Piece promoted) const noexcept;
    void add_capture_promotion(MoveList &ml, int from, int dest, Color color, Piece captured, Piece promoted) const noexcept;

    void push_quiet_moves(MoveList &ml, Bitboard attack, const int from);
    void push_capture_moves(MoveList &ml, Bitboard attack, const int from);
    void push_piece_quiet_moves(MoveList &ml, Bitboard attack, const int from, Color color, PieceType piece);
    void push_piece_capture_moves(MoveList &ml, Bitboard attack, const int from, Color color, PieceType piece);

    void push_quiet_promotions(MoveList &ml, Bitboard attack, const int dir, Color color);
    void push_capture_promotions(MoveList &ml, Bitboard attack, const int dir, Color color);
    void push_quiet_promotion(MoveList &ml, const int from, const int to, Color color);
    void push_capture_promotion(MoveList &ml, const int from, const int to, Color color);
    void push_pawn_quiet_moves(MoveList &ml, Bitboard attack, const int dir, Color color, U32 flags);
    void push_pawn_capture_moves(MoveList &ml, Bitboard attack, const int dir, Color color);

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
        if constexpr      (C == WHITE && side == CastleSide::KING_SIDE)
            return get_status().castling & CASTLE_WK;
        else if constexpr (C == WHITE && side == CastleSide::QUEEN_SIDE)
            return get_status().castling & CASTLE_WQ;
        else if constexpr (C == BLACK && side == CastleSide::KING_SIDE)
            return get_status().castling & CASTLE_BK;
        else if constexpr (C == BLACK && side == CastleSide::QUEEN_SIDE)
            return get_status().castling & CASTLE_BQ;
    }

    template <Color C, CastleSide side> constexpr Bitboard get_king_path()   // cases ne devant pas être attaquées
    {
        if constexpr      (C == WHITE && side == CastleSide::KING_SIDE)
            return F1G1_BB;
        else if constexpr (C == WHITE && side == CastleSide::QUEEN_SIDE)
            return C1D1_BB;
        else if constexpr (C == BLACK && side == CastleSide::KING_SIDE)
            return F8G8_BB;
        else if constexpr (C == BLACK && side == CastleSide::QUEEN_SIDE)
            return C8D8_BB;
    }

    template <Color C, CastleSide side> constexpr Bitboard get_rook_path()   // cases devant être libres
    {
        if constexpr      (C == WHITE && side == CastleSide::KING_SIDE)
            return F1G1_BB;
        else if constexpr (C == WHITE && side == CastleSide::QUEEN_SIDE)
            return B1D1_BB;
        else if constexpr (C == BLACK && side == CastleSide::KING_SIDE)
            return F8G8_BB;
        else if constexpr (C == BLACK && side == CastleSide::QUEEN_SIDE)
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
        if constexpr      (C == WHITE && side == CastleSide::KING_SIDE)
            return (G1);
        else if constexpr (C == WHITE && side == CastleSide::QUEEN_SIDE)
            return (C1);
        else if constexpr (C == BLACK && side == CastleSide::KING_SIDE)
            return (G8);
        else if constexpr (C == BLACK && side == CastleSide::QUEEN_SIDE)
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
            add_quiet_move(ml, get_king_from<C>(), get_king_dest<C, side>(), Move::make_piece(C, PieceType::KING), Move::FLAG_CASTLE_MASK);
        }
    }

    //=========================================================================

    template<Color C, bool Update_NNUE> void make_move(const MOVE move) noexcept;
    template<Color C, bool Update_NNUE> void undo_move() noexcept;
    template<Color C> void make_nullmove() noexcept;
    template<Color C> void undo_nullmove() noexcept;

    //! \brief  Retourne la couleur de la pièce située sur la case sq
    //! SUPPOSE qu'il y a une pièce sur cette case !!
    [[nodiscard]] constexpr Color color_on(const int sq) const noexcept
    {
        return( (colorPiecesBB[WHITE] & SQ::square_BB(sq)) ? WHITE : BLACK);
    }

    [[nodiscard]] constexpr Piece piece_at(const int sq) const noexcept
    {
        assert(sq != SQUARE_NONE);
        return pieceBoard[sq];
    }

    //! \brief  Retourne le type de la pièce située sur la case sq
    //! Routine servant au debug
    //! Sinon, utiliser piece_at
    [[nodiscard]] constexpr Piece piece_on(const int sq) const noexcept
    {
        for (Piece e : all_PIECE) {
            const PieceType pt = Move::type(e);
            if (typePiecesBB[static_cast<U32>(pt)] & SQ::square_BB(sq))
            {
                if (colorPiecesBB[Color::WHITE] & SQ::square_BB(sq))
                    return Move::make_piece(WHITE, pt);
                else
                    return Move::make_piece(BLACK, pt);
            }
        }
        return Piece::NONE;
    }
    [[nodiscard]] constexpr PieceType piecetype_on(const int sq) const noexcept
    {
        for (PieceType e : all_PIECE_TYPE) {
            if (typePiecesBB[static_cast<U32>(e)] & SQ::square_BB(sq)) {
                return e;
            }
        }
        return PieceType::NONE;
    }


    template <bool Update_NNUE> bool valid(const std::string& message) noexcept;
    [[nodiscard]] std::string display() const noexcept;

    template<Color C> Bitboard getNonPawnMaterial() const noexcept
    {
        return (  colorPiecesBB[C]
                ^ occupancy_cp<C, PieceType::PAWN>()
                ^ occupancy_cp<C, PieceType::KING>() ) ;
    }

    template<Color C> int getNonPawnMaterialCount() const noexcept
    {
        return (BB::count_bit(colorPiecesBB[C]
                              ^ occupancy_cp<C, PieceType::PAWN>()
                              ^ occupancy_cp<C, PieceType::KING>() ) );
    }

    //==============================================
    //  Evaluation
    int  evaluate();
    bool fast_see(const MOVE move, const int threshold) const;

    //====================================================================
    //! \brief  Détermine s'il y a eu 50 coups sans prise ni coup de pion
    //--------------------------------------------------------------------
    [[nodiscard]] inline bool fiftymoves() const noexcept { return get_status().fiftymove_counter >= 100; }

    //====================================================================
    //! \brief  Détermine s'il y a eu répétition de la même position
    //! Pour cela, on compare le hash code de la position.
    //! Voir Ethereal
    //! + lors du calcul de répétitions, il faut distinguer les positions de recherche,
    //!   et les positions de la partie.
    //--------------------------------------------------------------------
    [[nodiscard]] inline bool is_repetition(int ply) const noexcept
    {
        int reps             = 0;
        U64 current_key      = get_status().key;
        int gamemove_counter = StatusHistory.size() - 1;
        int halfmove_counter = get_status().fiftymove_counter;

        // Look through hash histories for our moves
        for (int i = gamemove_counter - 2; i >= 0; i -= 2)
        {
            // No draw can occur before a zeroing move
            if (i < gamemove_counter - halfmove_counter)
                break;

            // Check for matching hash with a two fold after the root,
            // or a three fold which occurs in part before the root move
            if (    StatusHistory[i].key == current_key
                && (   i > gamemove_counter - ply   // 2-fold : on considère des positions dans l'arbre de recherche
                    || ++reps == 2) )               // 3-fold : on considère toutes les positions de la partie
                return true;
        }

        return false;
    }


    //     /* position fen rnbq1rk1/pppp1ppn/4pb1p/8/2PP4/1P2PN2/PB2BPPP/RN1Q1RK1 b - - 0 8
    //      *     moves d7d6 d1c2 b7b6 b1c3 c8b7 d4d5 f8e8 d5e6  | h7g6 h8g8 g1h2 b7c6 c2e2
    //      *
    //      *          1    2    3    4    5    6    7    8    9      10   11   12   13   14        StatusHistory.size()
    //      *          0    1    2    3    4    5    6    7    8      9    10  >11<  12   13        gamemove_counter , 11 dans l'exemple
    //      *                                   i         i       R   i         C                   R = root, C = position cherchée
    //      *                                        0    1    2      3    4   >5<                  fiftymove_counter -> gamemove_counter - fiftymove_counter = 6
    //      *                                   5         7           9                             i
    //      *                                                   0     1    2   >3<   4    5         ply
    //      */


    //=============================================================================
    //! \brief  Détermine si la position est nulle
    //-----------------------------------------------------------------------------
    [[nodiscard]] inline bool is_draw(int ply) const noexcept { return ((is_repetition(ply) || fiftymoves())); }

    //=============================================================================
    //! \brief  Ajoute une pièce à la case indiquée
    //-----------------------------------------------------------------------------
    template <bool Update_NNUE> void add_piece(const int square, const Color color, const Piece piece) noexcept;
    void set_piece(const int square, const Color color, const Piece piece) noexcept;
    void move_piece(const int from, const int dest, const Color color, const Piece piece) noexcept;
    void remove_piece(const int square, const Color color, const Piece) noexcept;
    void capture_piece(const int from, const int dest, const Color color,
                       const Piece piece, const Piece captured) noexcept;
    void promotion_piece(const int from, const int dest, const Color color, const Piece promo) noexcept;
    void promocapt_piece(const int from, const int dest, const Color color, const Piece captured, const Piece promoted) noexcept;


    bool test_mirror(const std::string &line);
    template<Color C, bool divide> [[nodiscard]] std::uint64_t perft(const int depth) noexcept;
    void test_value(const std::string& fen );


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
    //  Données

    std::array<Bitboard, N_COLORS>     colorPiecesBB;   // bitboard des pièces pour chaque couleur
    std::array<Bitboard, N_PIECE_TYPE> typePiecesBB;    // bitboard des pièces pour chaque type de pièce
    std::array<Piece, N_SQUARES>       pieceBoard;      // donne la pièce occupant la case indiquée (type + couleur)
    std::array<int, N_COLORS>          x_king;          // position des rois
    Color side_to_move;                         // camp au trait
    std::vector<std::string> best_moves;        // meilleur coup (pour les tests tactiques)
    std::vector<std::string> avoid_moves;       // coup à éviter (pour les tests tactiques)
    NNUE nnue;                                  // réseau NNUE
    std::vector<Status> StatusHistory;          // historique de la partie (coups déjà joués ET coups de la recherche)


    //==============================================
    //  Status
    inline const Status& get_status() const { return StatusHistory.back(); }
    inline Status& get_status()             { return StatusHistory.back(); }

    [[nodiscard]] inline int get_fiftymove_counter() const noexcept { return get_status().fiftymove_counter; }
    [[nodiscard]] inline int get_fullmove_counter()  const noexcept { return get_status().fullmove_counter;  }
    [[nodiscard]] inline int get_ep_square()         const noexcept { return get_status().ep_square;         }
    [[nodiscard]] inline U64 get_key()               const noexcept { return get_status().key;               }
    [[nodiscard]] inline U64 get_pawn_key()          const noexcept { return get_status().pawn_key;          }
    [[nodiscard]] inline U64 get_mat_key(Color color) const noexcept { return get_status().mat_key[color];   }
    [[nodiscard]] inline Bitboard get_checkers()     const noexcept { return get_status().checkers;          }
    [[nodiscard]] inline Bitboard get_pinned()       const noexcept { return get_status().pinned;            }

    inline void reserve_capacity() {    // la capacité ne passe pas avec la copie
        nnue.reserve_capacity();
        StatusHistory.reserve(MAX_HISTO);
    }

};  // class Board





#endif
