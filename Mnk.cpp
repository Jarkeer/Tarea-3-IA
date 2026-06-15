

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <cstdlib>
#include <ctime>
#include <climits>
#include <string>
#include <iomanip>
#include <chrono>

using namespace std;
using namespace std::chrono;


// Metricas de un solo movimiento 
struct MoveStats {
  long long nodes_visited = 0;  
  long long nodes_pruned  = 0;  
  int       max_depth     = 0; 
  double    time_ms       = 0.0;
  long long memory_bytes  = 0;  
};

// Metricas acumuladas de toda una partida para un agente
struct AgentGameStats {
  long long total_nodes   = 0;
  long long total_pruned  = 0;
  int       max_depth     = 0;
  double    total_time_ms = 0.0;
  long long peak_memory   = 0;
  int       moves_made    = 0;
};

struct GameResult {
  int  winner;         
  int  turns;        
  AgentGameStats p1;   
  AgentGameStats p2;   
};

struct InputException { };

class State
{
public:
  enum Players { P2 = -1, P1 = 1 };

  
  static const char DISP_P1    = 'x';
  static const char DISP_P2    = 'o';
  static const char DISP_EMPTY = '-';

  // dimensiones del tablero 
  int M; // filas
  int N; // columnas
  int K; // longitud de linea para ganar

  
  State(int m, int n, int k) : M(m), N(n), K(k)
  {
    // Reservamos el tablero como vector de vectores
    sq.assign(M, vector<signed char>(N, 0));
    to_move = P1;
    filled = 0;
  }

  bool full() const
  {
    return filled >= M * N;
  }

  void print() const
  {
    cout << "\n====== M,N,K (" << M << "x" << N << ", K=" << K << ") ======\n";

    // encabezado de columnas
    cout << "   ";
    for (int x = 0; x < N; ++x)
      cout << setw(2) << x;
    cout << "\n";

    for (int y = 0; y < M; ++y) {
      cout << setw(2) << y << " ";
      for (int x = 0; x < N; ++x) {
        char c;
        if      (sq[y][x] == P1) c = DISP_P1;
        else if (sq[y][x] == P2) c = DISP_P2;
        else                     c = DISP_EMPTY;
        cout << " " << c;
      }
      cout << "\n";
    }
    cout << "================================\n";
    char turno = (to_move == P1) ? DISP_P1 : DISP_P2;
    cout << "Turno: " << turno << "  (Casillas usadas: " << filled << ")\n\n";
  }

  // Hace el movimiento (x=columna, y=fila) para el jugador actual
  bool make_move(int x, int y)
  {
    if (x < 0 || x >= N || y < 0 || y >= M) return false;
    if (sq[y][x] != 0) return false; 

    sq[y][x] = to_move;
    to_move = -to_move;
    ++filled;
    return true;
  }

  void undo_move(int x, int y)
  {
    sq[y][x] = 0;
    to_move = -to_move;
    --filled;
  }

  int get_to_move() const { return to_move; }

  // Retorna el valor de la casilla 
  signed char get_sq(int x, int y) const { return sq[y][x]; }

  int check_winner() const
  {
    // Revisamos las 4 direcciones posibles de una linea
    int dx[] = {1,  0, 1, -1};
    int dy[] = {0,  1, 1,  1};

    for (int y = 0; y < M; ++y) {
      for (int x = 0; x < N; ++x) {
        if (sq[y][x] == 0) continue; 
        int player = sq[y][x];

        for (int d = 0; d < 4; ++d) {
          int count = 1; 
          for (int step = 1; step < K; ++step) {
            int nx = x + dx[d] * step;
            int ny = y + dy[d] * step;
            if (nx < 0 || nx >= N || ny < 0 || ny >= M) break;
            if (sq[ny][nx] != player) break;
            ++count;
          }
          if (count >= K) return player; 
        }
      }
    }
    return 0; 
  }

  // Retorna todos los movimientos legales como pares 
  vector<pair<int,int>> get_moves() const
  {
    vector<pair<int,int>> moves;
    for (int y = 0; y < M; ++y)
      for (int x = 0; x < N; ++x)
        if (sq[y][x] == 0)
          moves.emplace_back(x, y);
    return moves;
  }

private:
  int to_move;                       
  vector<vector<signed char>> sq;     
  int filled;                         
};

//  HEURISTICA SIMPLE
int heuristic(const State& st, int player)
{
  int M   = st.M;
  int N   = st.N;
  int K   = st.K;
  int opp = -player;  

  int score = 0;

  // Revisamos las 4 direcciones
  int dx[] = {1,  0, 1, -1};
  int dy[] = {0,  1, 1,  1};

  for (int y = 0; y < M; ++y) {
    for (int x = 0; x < N; ++x) {
      for (int d = 0; d < 4; ++d) {

        // Revisamos si cabe una ventana de longitud K en esta direccion
        int ex = x + dx[d] * (K - 1);
        int ey = y + dy[d] * (K - 1);
        if (ex < 0 || ex >= N || ey < 0 || ey >= M) continue;

        // Contamos piezas del jugador y del oponente en esta ventana
        int my_pieces  = 0;
        int opp_pieces = 0;
        for (int step = 0; step < K; ++step) {
          int nx = x + dx[d] * step;
          int ny = y + dy[d] * step;
          if (st.get_sq(nx, ny) == player) ++my_pieces;
          if (st.get_sq(nx, ny) == opp)    ++opp_pieces;
        }

        if (opp_pieces == 0 && my_pieces > 0)
          score += my_pieces * my_pieces;

        if (my_pieces == 0 && opp_pieces > 0)
          score -= opp_pieces * opp_pieces;
      }
    }
  }
  return score;
}
//  AGENTE HUMANO

pair<int,int> human_player(const State& st)
{
  int x, y;
  while (true) {
    cout << "Ingrese su movimiento (columna fila): ";
    if (!(cin >> x >> y)) {
      cout << "Entrada invalida. Por favor ingrese dos enteros.\n";
      cin.clear();
      cin.ignore(1000, '\n');
      continue;
    }
    if (x < 0 || x >= st.N || y < 0 || y >= st.M) {
      cout << "Fuera de rango. Columna 0-" << st.N-1
           << ", Fila 0-"   << st.M-1 << "\n";
      continue;
    }
    if (st.get_sq(x, y) != 0) {
      cout << "Esa casilla ya esta ocupada. Elija otra.\n";
      continue;
    }
    return {x, y};
  }
}

//  AGENTE ALEATORIO
pair<int,int> random_agent(const State& st)
{
  auto moves = st.get_moves();
  int idx = rand() % (int)moves.size();
  return moves[idx];
}


// MINIMAX 
int minimax(State& st, int depth, int max_depth, bool is_maximizing, MoveStats& ms)
{
  ms.nodes_visited++;
  if (depth > ms.max_depth) ms.max_depth = depth;

  int winner = st.check_winner();
  if (winner == State::P1) return 1000 - depth;
  if (winner == State::P2) return depth - 1000;
  if (st.full()) return 0;

  if (depth >= max_depth)
    return heuristic(st, State::P1);

  if (is_maximizing) {
    int best = INT_MIN;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = minimax(st, depth + 1, max_depth, false, ms);
      st.undo_move(mv.first, mv.second);
      best = max(best, score);
    }
    return best;
  } else {
    int best = INT_MAX;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = minimax(st, depth + 1, max_depth, true, ms);
      st.undo_move(mv.first, mv.second);
      best = min(best, score);
    }
    return best;
  }
}

// Agente MinMax 
pair<int,int> minimax_agent(State& st, int max_depth, MoveStats& ms)
{
  pair<int,int> best_move = {-1, -1};
  int player = st.get_to_move();
  auto t0 = high_resolution_clock::now();

  if (player == State::P1) {
    int best_score = INT_MIN;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = minimax(st, 0, max_depth, false, ms);
      st.undo_move(mv.first, mv.second);
      if (score > best_score) { best_score = score; best_move = mv; }
    }
  } else {
    int best_score = INT_MAX;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = minimax(st, 0, max_depth, true, ms);
      st.undo_move(mv.first, mv.second);
      if (score < best_score) { best_score = score; best_move = mv; }
    }
  }
  auto t1 = high_resolution_clock::now();
  ms.time_ms = duration<double, milli>(t1 - t0).count();
  ms.memory_bytes = (long long)ms.max_depth * 256; // 256 bytes por frame de pila
  return best_move;
}


// NEGAMAX
int negamax(State& st, int depth, int max_depth, MoveStats& ms)
{
  ms.nodes_visited++;
  if (depth > ms.max_depth) ms.max_depth = depth;

  int winner = st.check_winner();
  if (winner != 0) return depth - 1000;
  if (st.full()) return 0;

  if (depth >= max_depth)
    return heuristic(st, st.get_to_move());

  int best = INT_MIN;
  for (auto& mv : st.get_moves()) {
    st.make_move(mv.first, mv.second);
    int score = -negamax(st, depth + 1, max_depth, ms);
    st.undo_move(mv.first, mv.second);
    best = max(best, score);
  }
  return best;
}

// Agente NEGAMAX 
pair<int,int> negamax_agent(State& st, int max_depth, MoveStats& ms)
{
  int best_score = INT_MIN;
  pair<int,int> best_move = {-1, -1};
  auto t0 = high_resolution_clock::now();

  for (auto& mv : st.get_moves()) {
    st.make_move(mv.first, mv.second);
    int score = -negamax(st, 0, max_depth, ms);
    st.undo_move(mv.first, mv.second);
    if (score > best_score) { best_score = score; best_move = mv; }
  }
  auto t1 = high_resolution_clock::now();
  ms.time_ms = duration<double, milli>(t1 - t0).count();
  ms.memory_bytes = (long long)ms.max_depth * 256;
  return best_move;
}


// ALPHABETA 
int alphabeta(State& st, int depth, int max_depth, int alpha, int beta,
              bool maximizing, MoveStats& ms)
{
  ms.nodes_visited++;
  if (depth > ms.max_depth) ms.max_depth = depth;

  int winner = st.check_winner();
  if (winner == State::P1) return 1000 - depth;
  if (winner == State::P2) return depth - 1000;
  if (st.full()) return 0;

  if (depth >= max_depth)
    return heuristic(st, State::P1);

  if (maximizing) {
    int best = INT_MIN;
    auto moves = st.get_moves();
    for (auto& mv : moves) {
      st.make_move(mv.first, mv.second);
      int score = alphabeta(st, depth+1, max_depth, alpha, beta, false, ms);
      st.undo_move(mv.first, mv.second);
      best  = max(best, score);
      alpha = max(alpha, score);
      if (beta <= alpha) {
        ms.nodes_pruned += (long long)(moves.size()); // resto del recorrido podado
        break;
      }
    }
    return best;
  } else {
    int best = INT_MAX;
    auto moves = st.get_moves();
    for (auto& mv : moves) {
      st.make_move(mv.first, mv.second);
      int score = alphabeta(st, depth+1, max_depth, alpha, beta, true, ms);
      st.undo_move(mv.first, mv.second);
      best = min(best, score);
      beta = min(beta, best);
      if (beta <= alpha) {
        ms.nodes_pruned += (long long)(moves.size());
        break;
      }
    }
    return best;
  }
}

// Agente AlphaBeta 
pair<int,int> alphabeta_agent(State& st, int max_depth, MoveStats& ms)
{
  pair<int,int> best_move = {-1, -1};
  int player = st.get_to_move();
  auto t0 = high_resolution_clock::now();

  if (player == State::P1) {
    int best_score = INT_MIN;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = alphabeta(st, 0, max_depth, INT_MIN, INT_MAX, false, ms);
      st.undo_move(mv.first, mv.second);
      if (score > best_score) { best_score = score; best_move = mv; }
    }
  } else {
    int best_score = INT_MAX;
    for (auto& mv : st.get_moves()) {
      st.make_move(mv.first, mv.second);
      int score = alphabeta(st, 0, max_depth, INT_MIN, INT_MAX, true, ms);
      st.undo_move(mv.first, mv.second);
      if (score < best_score) { best_score = score; best_move = mv; }
    }
  }
  auto t1 = high_resolution_clock::now();
  ms.time_ms = duration<double, milli>(t1 - t0).count();
  ms.memory_bytes = (long long)ms.max_depth * 256;
  return best_move;
}

string agent_name(int agent_id)
{
  switch (agent_id) {
    case 0: return "Humano";
    case 1: return "Aleatorio";
    case 2: return "MinMax";
    case 3: return "NegaMax";
    case 4: return "AlphaBeta";
    default: return "Desconocido";
  }
}

// Obtiene el movimiento del agente 
pair<int,int> get_move(int agent_id, State& st, int max_depth, MoveStats& ms)
{
  switch (agent_id) {
    case 0: return human_player(st);
    case 1: {
      auto t0 = high_resolution_clock::now();
      auto mv = random_agent(st);
      auto t1 = high_resolution_clock::now();
      ms.time_ms = duration<double,milli>(t1-t0).count();
      ms.nodes_visited = 1; ms.max_depth = 0;
      return mv;
    }
    case 2: return minimax_agent(st, max_depth, ms);
    case 3: return negamax_agent(st, max_depth, ms);
    case 4: return alphabeta_agent(st, max_depth, ms);
    default:
      cout << "Agente desconocido, usando aleatorio.\n";
      return random_agent(st);
  }
}


// Acumula las metricas de un movimiento en el acumulador del agente
static void accumulate(AgentGameStats& acc, const MoveStats& ms)
{
  acc.total_nodes   += ms.nodes_visited;
  acc.total_pruned  += ms.nodes_pruned;
  acc.total_time_ms += ms.time_ms;
  acc.moves_made++;
  if (ms.max_depth   > acc.max_depth)   acc.max_depth   = ms.max_depth;
  if (ms.memory_bytes > acc.peak_memory) acc.peak_memory = ms.memory_bytes;
}

static void print_agent_stats(const string& name, const AgentGameStats& s, bool is_alphabeta)
{
  int moves = max(s.moves_made, 1);
  cout << fixed << setprecision(3);
  cout << "  [" << name << "]\n";
  cout << "    Movimientos realizados : " << s.moves_made << "\n";
  cout << "    Nodos visitados (total): " << s.total_nodes
       << "  (prom/mov: " << s.total_nodes/moves << ")\n";
  if (is_alphabeta)
    cout << "    Nodos podados   (total): " << s.total_pruned << "\n";
  cout << "    Profundidad maxima alc.: " << s.max_depth << "\n";
  cout << "    Tiempo total           : " << s.total_time_ms << " ms"
       << "  (prom/mov: " << s.total_time_ms/moves << " ms)\n";
  cout << "    Memoria pico estimada  : " << s.peak_memory << " bytes\n";
}

GameResult play_game(int M, int N, int K, int H, int agent1, int agent2, bool verbose)
{
  State st(M, N, K);
  GameResult res;
  res.winner = 0;
  res.turns  = 0;

  if (verbose) {
    cout << "\n[PARTIDA] " << agent_name(agent1) << " (x) vs "
         << agent_name(agent2) << " (o)\n";
    st.print();
  }

  while (!st.full()) {
    int turn = st.get_to_move();
    int current_agent = (turn == State::P1) ? agent1 : agent2;

    MoveStats ms;
    pair<int,int> move = get_move(current_agent, st, H, ms);

    // Acumular en el agente correcto
    if (turn == State::P1) accumulate(res.p1, ms);
    else                   accumulate(res.p2, ms);
    res.turns++;

    if (verbose) {
      cout << "Turno " << res.turns
           << " | " << agent_name(current_agent)
           << " escoge: col=" << move.first << " fil=" << move.second
           << " | nodos=" << ms.nodes_visited;
      if (current_agent == 4) cout << " podados=" << ms.nodes_pruned;
      cout << " prof=" << ms.max_depth
           << " t=" << fixed << setprecision(2) << ms.time_ms << "ms\n";
    }

    st.make_move(move.first, move.second);
    if (verbose) st.print();

    int winner = st.check_winner();
    if (winner != 0) {
      if (verbose) {
        char w = (winner == State::P1) ? State::DISP_P1 : State::DISP_P2;
        string wn = (winner == State::P1) ? agent_name(agent1) : agent_name(agent2);
        cout << "Gana el jugador " << w << " (" << wn << ")!\n";
      }
      res.winner = winner;
      return res;
    }
  }

  if (verbose) cout << "Empate!\n";
  return res;
}


//  TORNEO 
void run_tournament(int M, int N, int K, int H)
{
  vector<int> agents = {1, 2, 3, 4};
  int n = (int)agents.size();

  // Contadores globales
  vector<int> total_wins(n, 0), total_draws(n, 0), total_losses(n, 0);
  vector<vector<int>> wins_vs(n, vector<int>(n, 0));

  // Metricas acumuladas
  vector<AgentGameStats> tour_stats(n);

  // Tabla detallada
  struct MatchRecord {
    string a1, a2;
    string result;
    int turns;
    AgentGameStats s1, s2;
    bool ab1, ab2;
  };
  vector<MatchRecord> records;

  cout << "\n";
  cout << "==============================================\n";
  cout << "   TORNEO M,N,K (" << M << "x" << N
       << ", K=" << K << ", H=" << H << ")\n";
  cout << "==============================================\n\n";

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (i == j) continue;

      cout << "  " << agent_name(agents[i])
           << " vs " << agent_name(agents[j]) << " ... "; cout.flush();

      GameResult gr = play_game(M, N, K, H, agents[i], agents[j], false);

      MatchRecord rec;
      rec.a1 = agent_name(agents[i]);
      rec.a2 = agent_name(agents[j]);
      rec.turns = gr.turns;
      rec.s1 = gr.p1; rec.s2 = gr.p2;
      rec.ab1 = (agents[i] == 4); rec.ab2 = (agents[j] == 4);

      if (gr.winner == State::P1) {
        wins_vs[i][j]++; total_wins[i]++; total_losses[j]++;
        rec.result = rec.a1 + " gana";
        cout << rec.a1 << " gana!\n";
      } else if (gr.winner == State::P2) {
        wins_vs[j][i]++; total_wins[j]++; total_losses[i]++;
        rec.result = rec.a2 + " gana";
        cout << rec.a2 << " gana!\n";
      } else {
        total_draws[i]++; total_draws[j]++;
        rec.result = "Empate";
        cout << "Empate!\n";
      }

      // Acumular estadisticas globales por agente
      auto& si = tour_stats[i];
      si.total_nodes   += gr.p1.total_nodes;
      si.total_pruned  += gr.p1.total_pruned;
      si.total_time_ms += gr.p1.total_time_ms;
      si.moves_made    += gr.p1.moves_made;
      if (gr.p1.max_depth   > si.max_depth)   si.max_depth   = gr.p1.max_depth;
      if (gr.p1.peak_memory > si.peak_memory) si.peak_memory = gr.p1.peak_memory;

      auto& sj = tour_stats[j];
      sj.total_nodes   += gr.p2.total_nodes;
      sj.total_pruned  += gr.p2.total_pruned;
      sj.total_time_ms += gr.p2.total_time_ms;
      sj.moves_made    += gr.p2.moves_made;
      if (gr.p2.max_depth   > sj.max_depth)   sj.max_depth   = gr.p2.max_depth;
      if (gr.p2.peak_memory > sj.peak_memory) sj.peak_memory = gr.p2.peak_memory;

      records.push_back(rec);
    }
  }

  // ---- TABLA DETALLADA POR PARTIDA ----
  cout << "\n";
  cout << "==========================================================\n";
  cout << "         DATOS TECNICOS POR PARTIDA\n";
  cout << "==========================================================\n";
  int gn = 1;
  for (auto& r : records) {
    int mv1 = max(r.s1.moves_made, 1);
    int mv2 = max(r.s2.moves_made, 1);
    cout << "\nPartida #" << gn++ << ": " << r.a1 << " (x) vs " << r.a2 << " (o)\n";
    cout << "  Resultado : " << r.result << "\n";
    cout << "  Turnos    : " << r.turns << "\n";
    cout << fixed << setprecision(3);
    cout << "  " << r.a1 << " (P1):\n";
    cout << "    Nodos visitados : " << r.s1.total_nodes
         << "  (prom/mov: " << r.s1.total_nodes/mv1 << ")\n";
    if (r.ab1)
      cout << "    Nodos podados   : " << r.s1.total_pruned << "\n";
    cout << "    Prof. maxima    : " << r.s1.max_depth << "\n";
    cout << "    Tiempo total    : " << r.s1.total_time_ms << " ms"
         << "  (prom/mov: " << r.s1.total_time_ms/mv1 << " ms)\n";
    cout << "    Memoria pico    : " << r.s1.peak_memory << " bytes\n";
    cout << "  " << r.a2 << " (P2):\n";
    cout << "    Nodos visitados : " << r.s2.total_nodes
         << "  (prom/mov: " << r.s2.total_nodes/mv2 << ")\n";
    if (r.ab2)
      cout << "    Nodos podados   : " << r.s2.total_pruned << "\n";
    cout << "    Prof. maxima    : " << r.s2.max_depth << "\n";
    cout << "    Tiempo total    : " << r.s2.total_time_ms << " ms"
         << "  (prom/mov: " << r.s2.total_time_ms/mv2 << " ms)\n";
    cout << "    Memoria pico    : " << r.s2.peak_memory << " bytes\n";
  }

  // TABLA DE VICTORIAS 
  cout << "\n==============================================\n";
  cout << "       RESULTADOS DEL TORNEO\n";
  cout << "==============================================\n\n";
  cout << "Victorias directas (fila gana a columna):\n\n";
  cout << setw(12) << " ";
  for (int j = 0; j < n; ++j) cout << setw(12) << agent_name(agents[j]);
  cout << "\n";
  for (int i = 0; i < n; ++i) {
    cout << setw(12) << agent_name(agents[i]);
    for (int j = 0; j < n; ++j) {
      if (i == j) cout << setw(12) << "-";
      else        cout << setw(12) << wins_vs[i][j];
    }
    cout << "\n";
  }

  //TABLA DE POSICIONES 
  int games_each = n - 1; 
  cout << "\nTabla de posiciones:\n";
  cout << "  " << setw(4) << "Pos"
       << setw(12) << "Agente"
       << setw(5)  << "PJ"
       << setw(5)  << "V"
       << setw(5)  << "E"
       << setw(5)  << "D"
       << setw(8)  << "%Victoria"
       << setw(8)  << "%Empate"
       << setw(8)  << "%Derrota" << "\n";
  cout << "  " << string(60, '-') << "\n";

  vector<int> ranking(n);
  for (int i = 0; i < n; ++i) ranking[i] = i;
  for (int i = 0; i < n; ++i)
    for (int j = i+1; j < n; ++j)
      if (total_wins[ranking[j]] > total_wins[ranking[i]])
        swap(ranking[i], ranking[j]);

  for (int pos = 0; pos < n; ++pos) {
    int idx = ranking[pos];
    int pj = total_wins[idx] + total_draws[idx] + total_losses[idx];
    double pv = pj ? 100.0 * total_wins[idx]   / pj : 0;
    double pe = pj ? 100.0 * total_draws[idx]  / pj : 0;
    double pd = pj ? 100.0 * total_losses[idx] / pj : 0;
    cout << fixed << setprecision(1);
    cout << "  " << setw(4) << (pos+1)
         << setw(12) << agent_name(agents[idx])
         << setw(5)  << pj
         << setw(5)  << total_wins[idx]
         << setw(5)  << total_draws[idx]
         << setw(5)  << total_losses[idx]
         << setw(8)  << pv
         << setw(8)  << pe
         << setw(8)  << pd << "\n";
  }

  cout << "\nMetricas tecnicas globales del torneo:\n";
  cout << string(60, '-') << "\n";
  for (int i = 0; i < n; ++i) {
    int mv = max(tour_stats[i].moves_made, 1);
    bool is_ab = (agents[i] == 4);
    cout << fixed << setprecision(3);
    cout << "[" << agent_name(agents[i]) << "]\n";
    cout << "  Total movimientos: " << tour_stats[i].moves_made << "\n";
    cout << "  Nodos visitados  : " << tour_stats[i].total_nodes
         << "  (prom/mov: " << tour_stats[i].total_nodes/mv << ")\n";
    if (is_ab)
      cout << "  Nodos podados    : " << tour_stats[i].total_pruned << "\n";
    cout << "  Prof. max alc.   : " << tour_stats[i].max_depth << "\n";
    cout << "  Tiempo total     : " << tour_stats[i].total_time_ms << " ms"
         << "  (prom/mov: " << tour_stats[i].total_time_ms/mv << " ms)\n";
    cout << "  Mem. pico estim. : " << tour_stats[i].peak_memory << " bytes\n";
  }
  cout << "==============================================\n";
}



int main(int argc, char* argv[])
{
  srand(time(0)); // semilla para el agente aleatorio

  int M = 3, N = 3, K = 3, H = 3;
  int agent1 = -1, agent2 = -1;

  if (argc >= 6 && string(argv[5]) == "torneo") {
    M = atoi(argv[1]);
    N = atoi(argv[2]);
    K = atoi(argv[3]);
    H = atoi(argv[4]);
    run_tournament(M, N, K, H);
    return 0;

  } else if (argc >= 7) {
    M      = atoi(argv[1]);
    N      = atoi(argv[2]);
    K      = atoi(argv[3]);
    H      = atoi(argv[4]);
    agent1 = atoi(argv[5]);
    agent2 = atoi(argv[6]);

  } else {
    cout << "=== JUEGO M,N,K ===\n";
    cout << "Instrucciones:\n";
    cout << "  Partida:  ./Mnk M N K H Agent1 Agent2\n";
    cout << "  Torneo:   ./Mnk M N K H torneo\n";
    cout << "  M, N   = dimensiones del tablero (filas x columnas)\n";
    cout << "  K      = cantidad de fichas en linea para ganar\n";
    cout << "  H      = profundidad de busqueda para los agentes IA\n";
    cout << "  Agentes: 0=Humano  1=Aleatorio  2=MinMax  3=NegaMax  4=AlphaBeta\n\n";

    cout << "Ingrese M (filas):       "; cin >> M;
    cout << "Ingrese N (columnas):    "; cin >> N;
    cout << "Ingrese K (para ganar):  "; cin >> K;
    cout << "Ingrese H (profundidad): "; cin >> H;

    cout << "\n¿Que desea hacer?\n";
    cout << "  1 - Jugar una partida\n";
    cout << "  2 - Ejecutar torneo automatico\n";
    cout << "Opcion: ";
    int modo; cin >> modo;

    if (modo == 2) {
      run_tournament(M, N, K, H);
      return 0;
    }

    cout << "\nAgentes disponibles:\n";
    cout << "  0 - Humano\n  1 - Aleatorio\n  2 - MinMax\n";
    cout << "  3 - NegaMax\n  4 - AlphaBeta\n";
    cout << "Agente para P1 (x): "; cin >> agent1;
    cout << "Agente para P2 (o): "; cin >> agent2;
  }

  // Validaciones basicas
  if (M <= 0 || N <= 0 || K <= 0 || H <= 0) {
    cout << "Error: M, N, K y H deben ser positivos.\n";
    return 1;
  }
  if (K > M && K > N) {
    cout << "Error: K es mayor que ambas dimensiones del tablero.\n";
    return 1;
  }
  if (agent1 < 0 || agent1 > 4 || agent2 < 0 || agent2 > 4) {
    cout << "Error: Los agentes deben ser 0, 1, 2, 3 o 4.\n";
    return 1;
  }

  cout << "\n=== PARTIDA: " << agent_name(agent1)
       << " (x) vs " << agent_name(agent2) << " (o) ===\n";
  cout << "Tablero: " << M << "x" << N
       << "  K=" << K << "  Profundidad=" << H << "\n";

  GameResult gr = play_game(M, N, K, H, agent1, agent2, true);

  // Mostrar resumen de metricas tecnicas de la partida
  cout << "\n=== METRICAS TECNICAS DE LA PARTIDA ===\n";
  cout << "Turnos jugados: " << gr.turns << "\n";
  print_agent_stats(agent_name(agent1) + " (P1)", gr.p1, agent1 == 4);
  print_agent_stats(agent_name(agent2) + " (P2)", gr.p2, agent2 == 4);
  cout << "=========================================\n";

  // Si no hay humano, ofrecemos correr el torneo automatico
  if (agent1 != 0 && agent2 != 0) {
    char resp;
    cout << "\n¿Desea ver el torneo automatico con estos parametros? (s/n): ";
    cin >> resp;
    if (resp == 's' || resp == 'S') {
      run_tournament(M, N, K, H);
    }
  }

  return 0;
}