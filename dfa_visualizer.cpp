#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <random>
#include <chrono>

extern "C" {
#include "lang.h"
#include "lexer.h"
}

using namespace Gdiplus;

namespace {
const double kPi = 3.14159265358979323846;

std::wstring to_wide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    std::vector<BYTE> buffer(size);
    ImageCodecInfo* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    GetImageEncoders(num, size, codecs);
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(codecs[i].MimeType, format) == 0) {
            *pClsid = codecs[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

char read_escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        default: return c;
    }
}

struct Parser {
    explicit Parser(const std::string& src) : text(src), pos(0) {}

    struct frontend_regexp* parse() {
        auto* result = parse_union();
        skip_spaces();
        if (pos != text.size()) {
            throw std::runtime_error("Unexpected trailing characters in regex.");
        }
        return result;
    }

private:
    const std::string text;
    size_t pos;

    bool eof() const { return pos >= text.size(); }
    char peek() const { return eof() ? '\0' : text[pos]; }
    char advance() { return eof() ? '\0' : text[pos++]; }

    void skip_spaces() {
        while (!eof() && isspace(static_cast<unsigned char>(peek()))) pos++;
    }

    frontend_regexp* parse_union() {
        frontend_regexp* left = parse_concat();
        skip_spaces();
        while (peek() == '|') {
            advance();
            skip_spaces();
            frontend_regexp* right = parse_concat();
            left = TFr_Union(left, right);
            skip_spaces();
        }
        return left;
    }

    frontend_regexp* parse_concat() {
        frontend_regexp* left = parse_repeat();
        skip_spaces();
        while (!eof() && peek() != ')' && peek() != '|') {
            frontend_regexp* right = parse_repeat();
            left = TFr_Concat(left, right);
            skip_spaces();
        }
        return left;
    }

    frontend_regexp* parse_repeat() {
        frontend_regexp* atom = parse_atom();
        skip_spaces();
        while (!eof()) {
            char c = peek();
            if (c == '*') {
                advance();
                atom = TFr_Star(atom);
            } else if (c == '+') {
                advance();
                atom = TFr_Plus(atom);
            } else if (c == '?') {
                advance();
                atom = TFr_Option(atom);
            } else {
                break;
            }
            skip_spaces();
        }
        return atom;
    }

    frontend_regexp* parse_atom() {
        skip_spaces();
        if (eof()) throw std::runtime_error("Unexpected end of regex.");
        char c = peek();
        if (c == '(') {
            advance();
            frontend_regexp* inner = parse_union();
            if (peek() != ')') throw std::runtime_error("Missing closing parenthesis.");
            advance();
            return inner;
        }
        if (c == '[') {
            struct char_set* cs = parse_char_set();
            return TFr_CharSet(cs);
        }
        if (c == '"') {
            std::string s = parse_string_literal();
            return TFr_String(s.data());
        }
        if (c == '\\') {
            advance();
            if (eof()) throw std::runtime_error("Dangling escape.");
            char escaped = read_escape(advance());
            return TFr_SingleChar(escaped);
        }
        if (c == '|' || c == ')' ) {
            throw std::runtime_error("Unexpected operator position.");
        }
        advance();
        return TFr_SingleChar(c);
    }

    std::string parse_string_literal() {
        if (peek() != '"') throw std::runtime_error("String literal must start with \".");
        advance(); // consume "
        std::string out;
        while (!eof() && peek() != '"') {
            out.push_back(advance());
        }
        if (peek() != '"') throw std::runtime_error("Missing closing quote for string literal.");
        advance(); // consume closing "
        return out;
    }

    struct char_set* parse_char_set() {
        if (peek() != '[') throw std::runtime_error("Character set must start with '['.");
        advance();
        std::vector<unsigned char> chars;
        bool closed = false;
        while (!eof()) {
            char c = advance();
            if (c == ']') { closed = true; break; }
            if (c == '\\') {
                if (eof()) throw std::runtime_error("Dangling escape in character set.");
                c = read_escape(advance());
            }
            if (peek() == '-' && (pos + 1) < text.size() && text[pos + 1] != ']') {
                advance(); // consume '-'
                char end_ch = advance();
                if (end_ch == '\\') {
                    if (eof()) throw std::runtime_error("Dangling escape in character set range.");
                    end_ch = read_escape(advance());
                }
                if (end_ch < c) std::swap(end_ch, c);
                for (unsigned char ch = static_cast<unsigned char>(c); ch <= static_cast<unsigned char>(end_ch); ++ch) {
                    chars.push_back(ch);
                }
            } else {
                chars.push_back(static_cast<unsigned char>(c));
            }
        }
        if (!closed) throw std::runtime_error("Missing closing ']' for character set.");
        auto* cs = static_cast<char_set*>(malloc(sizeof(char_set)));
        cs->n = static_cast<unsigned int>(chars.size());
        cs->c = static_cast<char*>(malloc(cs->n));
        for (unsigned int i = 0; i < cs->n; i++) {
            cs->c[i] = static_cast<char>(chars[i]);
        }
        return cs;
    }
};

std::string printable_char(unsigned char c) {
    switch (c) {
        case '\n': return "\\n";
        case '\t': return "\\t";
        case '\r': return "\\r";
        case '\\': return "\\\\";
        case '"': return "\\\"";
        case '\'': return "\\'";
        case ' ': return " ";
        default:
            if (isprint(c)) {
                return std::string(1, static_cast<char>(c));
            }
            std::ostringstream oss;
            oss << "\\x" << std::hex << std::uppercase << static_cast<int>(c);
            return oss.str();
    }
}

std::string format_characters_only(const std::vector<unsigned char>& chars) {
    if (chars.empty()) return "";
    std::vector<unsigned char> sorted = chars;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::ostringstream oss;
    bool multiple = sorted.size() > 1;
    if (multiple) oss << "[";
    for (size_t i = 0; i < sorted.size(); i++) {
        unsigned char start = sorted[i];
        unsigned char end = start;
        while (i + 1 < sorted.size() && sorted[i + 1] == static_cast<unsigned char>(end + 1)) {
            end = sorted[++i];
        }
        if (start == end) {
            oss << printable_char(start);
        } else {
            oss << printable_char(start) << "-" << printable_char(end);
        }
    }
    if (multiple) oss << "]";
    return oss.str();
}

std::string format_charset(const std::vector<unsigned char>& chars, bool epsilon) {
    if (epsilon) return "eps";
    if (chars.empty()) return "";
    std::vector<unsigned char> sorted = chars;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::vector<unsigned char> char_only;
    std::vector<unsigned char> string_tokens;
    for (unsigned char c : sorted) {
        if (is_string_token_char(c)) {
            string_tokens.push_back(c);
        } else {
            char_only.push_back(c);
        }
    }

    std::vector<std::string> parts;
    if (!char_only.empty()) {
        parts.push_back(format_characters_only(char_only));
    }
    for (unsigned char token : string_tokens) {
        const char* label = get_string_token_label(token);
        if (label) {
            std::ostringstream os;
            os << "\"" << label << "\"";
            parts.push_back(os.str());
        } else {
            parts.push_back(printable_char(token));
        }
    }

    if (parts.empty()) return "";
    if (parts.size() == 1) return parts[0];
    std::ostringstream merged;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) merged << " | ";
        merged << parts[i];
    }
    return merged.str();
}

struct EdgeAggregate {
    int src;
    int dst;
    bool epsilon;
    std::vector<unsigned char> chars;
};

std::vector<EdgeAggregate> aggregate_edges(struct finite_automata* dfa) {
    std::vector<EdgeAggregate> edges;
    for (int e = 0; e < dfa->m; e++) {
        bool eps = dfa->lb[e].n == 0;
        int src = dfa->src[e];
        int dst = dfa->dst[e];
        auto it = std::find_if(edges.begin(), edges.end(), [&](const EdgeAggregate& ag) {
            return ag.src == src && ag.dst == dst && ag.epsilon == eps;
        });
        if (it == edges.end()) {
            EdgeAggregate ag;
            ag.src = src;
            ag.dst = dst;
            ag.epsilon = eps;
            if (!eps) {
                for (unsigned int i = 0; i < dfa->lb[e].n; i++) {
                    ag.chars.push_back(static_cast<unsigned char>(dfa->lb[e].c[i]));
                }
            }
            edges.push_back(ag);
        } else if (!eps) {
            for (unsigned int i = 0; i < dfa->lb[e].n; i++) {
                it->chars.push_back(static_cast<unsigned char>(dfa->lb[e].c[i]));
            }
        }
    }
    return edges;
}

std::vector<PointF> force_layout(int n, const std::vector<EdgeAggregate>& edges, int width, int height, float radius) {
    std::vector<PointF> pos(n);
    std::vector<PointF> disp(n);
    if (n == 0) return pos;

    // random initialization inside a central box
    std::mt19937 rng(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_real_distribution<float> ux(width * 0.25f, width * 0.75f);
    std::uniform_real_distribution<float> uy(height * 0.25f, height * 0.75f);
    for (int i = 0; i < n; ++i) {
        pos[i] = PointF(ux(rng), uy(rng));
    }

    float area = static_cast<float>(width * height);
    float k = std::sqrt(area / std::max(1, n));
    float margin = radius + 80.0f;
    int iterations = 300;
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n; ++i) disp[i] = PointF(0, 0);

        // repulsive
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                float dx = pos[i].X - pos[j].X;
                float dy = pos[i].Y - pos[j].Y;
                float dist = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
                float force = (k * k) / dist;
                float fx = force * dx / dist;
                float fy = force * dy / dist;
                disp[i].X += fx; disp[i].Y += fy;
                disp[j].X -= fx; disp[j].Y -= fy;
            }
        }

        // attractive along edges
        for (const auto& e : edges) {
            int v = e.src;
            int u = e.dst;
            if (v == u) continue;
            float dx = pos[v].X - pos[u].X;
            float dy = pos[v].Y - pos[u].Y;
            float dist = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
            float force = (dist * dist) / k;
            float fx = force * dx / dist;
            float fy = force * dy / dist;
            disp[v].X -= fx; disp[v].Y -= fy;
            disp[u].X += fx; disp[u].Y += fy;
        }

        float t = k * 0.6f * (1.0f - static_cast<float>(iter) / iterations);
        for (int i = 0; i < n; ++i) {
            float dx = disp[i].X;
            float dy = disp[i].Y;
            float dist = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
            float scale = std::min(t, dist) / dist;
            pos[i].X += dx * scale;
            pos[i].Y += dy * scale;

            pos[i].X = std::min(static_cast<float>(width) - margin, std::max(margin, pos[i].X));
            pos[i].Y = std::min(static_cast<float>(height) - margin, std::max(margin, pos[i].Y));
        }
    }
    return pos;
}

void draw_state(Graphics& g, const PointF& pos, float radius, int id, bool accepting) {
    Color outline(255, 40, 40, 40);
    Color fill(255, 240, 244, 255);
    Pen pen(outline, 2.0f);
    SolidBrush brush(fill);
    g.FillEllipse(&brush, pos.X - radius, pos.Y - radius, radius * 2.0f, radius * 2.0f);
    g.DrawEllipse(&pen, pos.X - radius, pos.Y - radius, radius * 2.0f, radius * 2.0f);
    if (accepting) {
        g.DrawEllipse(&pen, pos.X - radius + 5.0f, pos.Y - radius + 5.0f, (radius - 5.0f) * 2.0f, (radius - 5.0f) * 2.0f);
    }

    std::string id_text = std::to_string(id + 1); // display starts from 1
    std::wstring wid = to_wide(id_text);
    FontFamily font_family(L"Arial");
    Font font(&font_family, 15, FontStyleBold, UnitPixel);
    RectF layout(pos.X - radius, pos.Y - radius, radius * 2.0f, radius * 2.0f);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    SolidBrush text_brush(Color(255, 20, 20, 20));
    g.DrawString(wid.c_str(), -1, &font, layout, &format, &text_brush);
}

void draw_edge(Graphics& g, const PointF& from, const PointF& to, float state_radius, const std::string& label, bool self_loop, const std::vector<PointF>& nodes, int src, int dst, bool has_reverse) {
    AdjustableArrowCap arrow(6.0f, 8.0f, TRUE);
    Pen pen(Color(255, 50, 50, 50), 2.0f);
    pen.SetCustomEndCap(&arrow);
    if (self_loop) {
        float ang_start = -140.0f;
        float ang_end = -40.0f;
        float cr = state_radius;
        PointF p0(
            static_cast<REAL>(from.X + cr * std::cos(ang_start * kPi / 180.0)),
            static_cast<REAL>(from.Y + cr * std::sin(ang_start * kPi / 180.0)));
        PointF p3(
            static_cast<REAL>(from.X + cr * std::cos(ang_end * kPi / 180.0)),
            static_cast<REAL>(from.Y + cr * std::sin(ang_end * kPi / 180.0)));
        PointF c1(
            static_cast<REAL>(from.X - cr * 1.2f),
            static_cast<REAL>(from.Y - cr * 1.8f));
        PointF c2(
            static_cast<REAL>(from.X + cr * 1.2f),
            static_cast<REAL>(from.Y - cr * 1.8f));

        GraphicsPath path;
        path.AddBezier(p0, c1, c2, p3);

        AdjustableArrowCap loop_arrow(7.0f, 9.0f, TRUE);
        Pen loop_pen(Color(255, 50, 50, 50), 2.0f);
        loop_pen.SetCustomEndCap(&loop_arrow);
        g.DrawPath(&loop_pen, &path);

        // midpoint for label (t = 0.5 on cubic Bezier)
        auto bezier_mid = [](const PointF& a, const PointF& b, const PointF& c, const PointF& d) {
            float t = 0.5f;
            float mt = 1.0f - t;
            float x = mt*mt*mt*a.X + 3*mt*mt*t*b.X + 3*mt*t*t*c.X + t*t*t*d.X;
            float y = mt*mt*mt*a.Y + 3*mt*mt*t*b.Y + 3*mt*t*t*c.Y + t*t*t*d.Y;
            return PointF(x, y);
        };
        PointF label_pos = bezier_mid(p0, c1, c2, p3);
        label_pos.Y -= 6.0f; // lift label to reduce overlap

        FontFamily font_family(L"Arial");
        Font font(&font_family, 14, FontStyleRegular, UnitPixel);
        SolidBrush brush(Color(255, 0, 0, 0));
        RectF layout(label_pos.X - 80.0f, label_pos.Y - 16.0f, 160.0f, 32.0f);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(to_wide(label).c_str(), -1, &font, layout, &format, &brush);
        return;
    }
    double dx = to.X - from.X;
    double dy = to.Y - from.Y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3) return;
    double nx = dx / len;
    double ny = dy / len;
    PointF start(static_cast<REAL>(from.X + nx * state_radius), static_cast<REAL>(from.Y + ny * state_radius));
    PointF end(static_cast<REAL>(to.X - nx * state_radius), static_cast<REAL>(to.Y - ny * state_radius));

    // detect if straight segment intersects other node disks
    auto dist_point_seg = [](const PointF& p, const PointF& a, const PointF& b) -> double {
        double vx = b.X - a.X;
        double vy = b.Y - a.Y;
        double wx = p.X - a.X;
        double wy = p.Y - a.Y;
        double c1 = vx * wx + vy * wy;
        auto sqdist = [](double dx, double dy) { return dx * dx + dy * dy; };
        if (c1 <= 0) return std::sqrt(sqdist(p.X - a.X, p.Y - a.Y));
        double c2 = vx * vx + vy * vy;
        if (c2 <= c1) return std::sqrt(sqdist(p.X - b.X, p.Y - b.Y));
        double t = c1 / c2;
        double projx = a.X + t * vx;
        double projy = a.Y + t * vy;
        return std::sqrt(sqdist(p.X - projx, p.Y - projy));
    };

    bool need_curve = false;
    double clearance = state_radius * 1.05;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (static_cast<int>(i) == src || static_cast<int>(i) == dst) continue;
        if (dist_point_seg(nodes[i], start, end) < clearance) {
            need_curve = true;
            break;
        }
    }

    GraphicsPath path;
    PointF label_mid;
    PointF normal(static_cast<REAL>(-ny), static_cast<REAL>(nx));
    bool use_curve = need_curve;
    if (use_curve) {
        PointF mid(static_cast<REAL>((start.X + end.X) * 0.5f), static_cast<REAL>((start.Y + end.Y) * 0.5f));
        float base_offset = state_radius * 2.0f;
        float extra = static_cast<float>((static_cast<int>(std::fabs(from.X + from.Y + to.X + to.Y)) % 8)) * 3.0f; // 0~21
        float curve_offset = base_offset + extra;
        PointF ctrl(static_cast<REAL>(mid.X + normal.X * curve_offset),
                    static_cast<REAL>(mid.Y + normal.Y * curve_offset));
        path.AddBezier(start, ctrl, ctrl, end);
        auto bezier_mid = [](const PointF& a, const PointF& b, const PointF& c, const PointF& d) {
            float t = 0.5f;
            float mt = 1.0f - t;
            float x = mt*mt*mt*a.X + 3*mt*mt*t*b.X + 3*mt*t*t*c.X + t*t*t*d.X;
            float y = mt*mt*mt*a.Y + 3*mt*mt*t*b.Y + 3*mt*t*t*c.Y + t*t*t*d.Y;
            return PointF(x, y);
        };
        label_mid = bezier_mid(start, ctrl, ctrl, end);
    } else {
        path.AddLine(start, end);
        label_mid = PointF(static_cast<REAL>((start.X + end.X) * 0.5f), static_cast<REAL>((start.Y + end.Y) * 0.5f));
    }

    // 标签额外偏移，避免双向边文字重叠（即便走直线也偏移）
    if (has_reverse) {
        float label_shift_n = 12.0f * (src > dst ? -1.0f : 1.0f); // 区分方向
        float label_shift_t = 8.0f; // 沿切向微移
        label_mid.X += normal.X * label_shift_n + static_cast<REAL>(nx * label_shift_t);
        label_mid.Y += normal.Y * label_shift_n + static_cast<REAL>(ny * label_shift_t);
    }

    g.DrawPath(&pen, &path);

    FontFamily font_family(L"Arial");
    Font font(&font_family, 14, FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 0, 0, 0));
    RectF layout(label_mid.X - 110.0f, label_mid.Y - 14.0f, 220.0f, 28.0f);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(to_wide(label).c_str(), -1, &font, layout, &format, &brush);
}

void render_dfa(struct finite_automata* dfa, int* accepting_rules, const std::string& filename) {
    const int width = 1400;
    const int height = 900;
    const float radius = 34.0f;

    Bitmap bitmap(width, height, PixelFormat32bppARGB);
    Graphics g(&bitmap);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.Clear(Color(255, 255, 255, 255));
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    auto edges = aggregate_edges(dfa);
    std::vector<PointF> positions = force_layout(dfa->n, edges, width, height, radius);
    if (dfa->n > 0) {
        float min_x = positions[0].X, max_x = positions[0].X;
        float min_y = positions[0].Y, max_y = positions[0].Y;
        for (int i = 1; i < dfa->n; ++i) {
            min_x = std::min(min_x, positions[i].X);
            max_x = std::max(max_x, positions[i].X);
            min_y = std::min(min_y, positions[i].Y);
            max_y = std::max(max_y, positions[i].Y);
        }
        float cx = (min_x + max_x) * 0.5f;
        float cy = (min_y + max_y) * 0.5f;
        float target_cx = width * 0.5f;
        float target_cy = height * 0.5f;
        float shift_x = target_cx - cx;
        float shift_y = target_cy - cy;
        for (int i = 0; i < dfa->n; ++i) {
            positions[i].X += shift_x;
            positions[i].Y += shift_y;
        }
    }
    for (const auto& e : edges) {
        std::string label = format_charset(e.chars, e.epsilon);
        bool self_loop = e.src == e.dst;
        bool has_reverse = false;
        if (!self_loop) {
            for (const auto& other : edges) {
                if (other.src == e.dst && other.dst == e.src && other.epsilon == e.epsilon) {
                    has_reverse = true;
                    break;
                }
            }
        }
        draw_edge(g, positions[e.src], positions[e.dst], radius, label, self_loop, positions, e.src, e.dst, has_reverse);
    }

    for (int i = 0; i < dfa->n; i++) {
        bool accepting = accepting_rules && accepting_rules[i] != -1;
        draw_state(g, positions[i], radius, i, accepting);
    }

    // Start arrow
    if (dfa->n > 0) {
        AdjustableArrowCap arrow(6.0f, 8.0f, TRUE);
        Pen pen(Color(255, 30, 30, 30), 2.0f);
        pen.SetCustomEndCap(&arrow);
        PointF to(positions[0].X - radius * 0.8f, positions[0].Y - radius * 0.2f);
        PointF from(positions[0].X - radius * 3.0f, positions[0].Y - radius * 1.6f);
        g.DrawLine(&pen, from, to);

        // Label the start arrow with INIT, offset slightly to avoid overlap
        PointF mid(static_cast<REAL>((from.X + to.X) * 0.5f), static_cast<REAL>((from.Y + to.Y) * 0.5f));
        mid.Y -= 8.0f;
        FontFamily font_family(L"Arial");
        Font font(&font_family, 12, FontStyleBold, UnitPixel);
        SolidBrush brush(Color(255, 0, 0, 0));
        RectF layout(mid.X - 44.0f, mid.Y - 12.0f, 88.0f, 24.0f);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(L"INIT", -1, &font, layout, &format, &brush);
    }

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) {
        throw std::runtime_error("PNG encoder not found.");
    }
    std::wstring wpath = to_wide(filename);
    Status status = bitmap.Save(wpath.c_str(), &pngClsid, nullptr);
    if (status != Ok) {
        throw std::runtime_error("Failed to save PNG file.");
    }
}

}  // namespace

int main() {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        std::cerr << "Failed to initialize GDI+." << std::endl;
        return 1;
    }

    std::cout << "Enter regex (type 'quit' to exit). Each render saves dfa_<n>.png" << std::endl;
    std::string input;
    int vis_index = 1;
    while (true) {
        std::cout << "Regex> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "quit") break;
        if (input.empty()) continue;
        try {
            reset_string_token_table();
            Parser parser(input);
            frontend_regexp* fr = parser.parse();
            simpl_regexp* sr = simplify_regexp(fr);

            finite_automata* nfa = create_empty_graph();
            NFAFragment frag = regexp_to_nfa_fragment(nfa, sr);
            int accepting_states[1] = {frag.end};
            std::vector<int> dfa_accepting_rules(1000, -1);
            finite_automata* dfa = nfa_to_dfa(nfa, accepting_states, 1, dfa_accepting_rules.data());

            std::string filename = "dfa_" + std::to_string(vis_index++) + ".png";
            render_dfa(dfa, dfa_accepting_rules.data(), filename);
            std::cout << "Saved: " << filename << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}

