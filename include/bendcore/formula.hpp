// Параметрические размеры с поддержкой формул (Q3 2027, Task 1)
// Система параметрических размеров для чертежей труб с поддержкой:
// - Арифметических выражений: +, -, *, /, ^, sqrt(), abs(), min(), max()
// - Переменных трубы: $OD, $Wall, $Length, $BendAngle, $BendRadius
// - Ссылок на спецификации: =BOM.B5, =Spec.C3
// - Автоматического пересчёта при изменении пути
// - Вычисления по формуле Ньютона-Рафсона для неявных уравнений
#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace bendcore::formula {

// ---------- AST (Abstract Syntax Tree) ----------

enum class UnaryOp { Neg, Abs, Sqrt, Sin, Cos, Tan, Floor, Ceil };
enum class BinaryOp { Add, Sub, Mul, Div, Pow, Min, Max };

// Узел AST может быть: число, переменная, унарная операция, бинарная операция,
// вызов функции или ссылка на ячейку спецификации
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

struct ASTNode {
    enum Type {
        Number,
        Variable,
        UnaryOpNode,
        BinaryOpNode,
        FunctionCall,
        SpecReference
    } type;

    // Number
    double number_value = 0.0;

    // Variable (e.g., "$OD", "$Wall")
    std::string variable_name;

    // UnaryOp
    UnaryOp unary_op = UnaryOp::Neg;
    ASTNodePtr unary_operand;

    // BinaryOp
    BinaryOp binary_op = BinaryOp::Add;
    ASTNodePtr left_operand;
    ASTNodePtr right_operand;

    // FunctionCall (e.g., "min($OD, 100)")
    std::string function_name;
    std::vector<ASTNodePtr> function_args;

    // SpecReference (e.g., "BOM.B5")
    std::string spec_table_name;     // "BOM", "Spec", etc.
    char spec_column = 'A';          // A-Z
    int spec_row = 0;                // 1-based row number

    ASTNode() = default;

    static ASTNodePtr make_number(double value) {
        auto node = std::make_shared<ASTNode>();
        node->type = Number;
        node->number_value = value;
        return node;
    }

    static ASTNodePtr make_variable(const std::string& name) {
        auto node = std::make_shared<ASTNode>();
        node->type = Variable;
        node->variable_name = name;
        return node;
    }

    static ASTNodePtr make_unary(UnaryOp op, ASTNodePtr operand) {
        auto node = std::make_shared<ASTNode>();
        node->type = UnaryOpNode;
        node->unary_op = op;
        node->unary_operand = operand;
        return node;
    }

    static ASTNodePtr make_binary(BinaryOp op, ASTNodePtr left, ASTNodePtr right) {
        auto node = std::make_shared<ASTNode>();
        node->type = BinaryOpNode;
        node->binary_op = op;
        node->left_operand = left;
        node->right_operand = right;
        return node;
    }

    static ASTNodePtr make_function(const std::string& name, const std::vector<ASTNodePtr>& args) {
        auto node = std::make_shared<ASTNode>();
        node->type = FunctionCall;
        node->function_name = name;
        node->function_args = args;
        return node;
    }

    static ASTNodePtr make_spec_reference(const std::string& table_name, char column, int row) {
        auto node = std::make_shared<ASTNode>();
        node->type = SpecReference;
        node->spec_table_name = table_name;
        node->spec_column = column;
        node->spec_row = row;
        return node;
    }
};

// ---------- Парсер формул ----------

class FormulaParser {
public:
    // Парсит строку формулы и возвращает AST
    // Примеры: "5 + 3", "$OD * 2", "sqrt($Wall)", "=BOM.B5"
    static ASTNodePtr parse(const std::string& formula_text);

    class Tokenizer {
    public:
        enum class TokenType {
            Number,
            Identifier,
            Plus,
            Minus,
            Star,
            Slash,
            Caret,
            LParen,
            RParen,
            Comma,
            Dollar,
            Equal,
            Dot,
            Eof
        };

        struct Token {
            TokenType type;
            std::string value;
            size_t pos;
        };

        explicit Tokenizer(const std::string& text) : text_(text), pos_(0) {}

        Token next_token();
        Token peek_token();

    private:
        const std::string& text_;
        size_t pos_;
        std::optional<Token> peeked_;

        void skip_whitespace();
        bool is_digit(char c) const;
        bool is_alpha(char c) const;
        bool is_alnum(char c) const;
    };

private:
    explicit FormulaParser(const std::string& text) : tokenizer_(text), current_token_({Tokenizer::TokenType::Eof, "", 0}) {
        advance();
    }

    void advance();
    ASTNodePtr parse_expression();
    ASTNodePtr parse_additive();
    ASTNodePtr parse_multiplicative();
    ASTNodePtr parse_power();
    ASTNodePtr parse_unary();
    ASTNodePtr parse_postfix();
    ASTNodePtr parse_primary();
    ASTNodePtr parse_spec_reference();
    ASTNodePtr parse_function_call(const std::string& func_name);

    void expect(Tokenizer::TokenType type);
    bool match(Tokenizer::TokenType type);
    bool check(Tokenizer::TokenType type) const;

    Tokenizer tokenizer_;
    Tokenizer::Token current_token_;
};

// ---------- Ссылка на ячейку спецификации ----------

struct SpecificationReference {
    std::string table_name;  // "BOM", "Spec", etc.
    char column = 'A';       // A-Z
    int row = 0;             // 1-based

    std::string address() const;  // "B5", "C3", etc.
};

// Функция для парсинга ссылки из строки типа "B5"
SpecificationReference parse_spec_address(const std::string& address);

// ---------- Evaluator ----------

struct EvaluationContext {
    // Переменные трубы и чертежа
    std::map<std::string, double> variables;

    // Ссылки на ячейки спецификации (таблица -> строка -> столбец -> значение)
    std::map<std::string, std::map<int, std::map<char, double>>> specification_tables;

    // Для отладки и сообщений об ошибках
    std::string last_error;
};

class FormulaEvaluator {
public:
    // Вычисляет значение формулы при заданном контексте
    static double evaluate(const ASTNodePtr& node, EvaluationContext& context);

    // Вычисляет формулу из строки
    static double evaluate_formula(const std::string& formula_text, EvaluationContext& context);

    // Проверяет для циклических ссылок на спецификации
    static bool has_circular_reference(const ASTNodePtr& node,
                                      const std::vector<SpecificationReference>& already_evaluating);

private:
    static double eval_node(const ASTNodePtr& node, EvaluationContext& context);
    static double eval_unary(UnaryOp op, double operand);
    static double eval_binary(BinaryOp op, double left, double right);
};

// ---------- Решатель Ньютона-Рафсона для неявных уравнений ----------

class NewtonRaphsonSolver {
public:
    struct SolverOptions {
        double tolerance;      // точность
        int max_iterations;    // максимум итераций
        double initial_guess;  // начальное приближение
        double step_size;      // шаг для численного дифференцирования

        SolverOptions() : tolerance(1e-6), max_iterations(100), initial_guess(1.0), step_size(1e-6) {}
    };

    struct SolverResult {
        bool converged;
        double value;
        int iterations;
        std::string error;

        SolverResult() : converged(false), value(0.0), iterations(0), error("") {}
    };

    // Решает уравнение f(x) = target
    // equation — функция, принимающая значение x
    // target — целевое значение (по умолчанию 0)
    static SolverResult solve(
        const std::function<double(double)>& equation,
        const SolverOptions& options = SolverOptions(),
        double target = 0.0
    );
};

// ---------- Размер с формулой (Task 1.3-1.4) ----------

struct DimensionFormula {
    std::string id;                  // уникальный идентификатор размера
    std::string formula_text;        // текст формулы, например "$OD * 2 + 5"
    ASTNodePtr formula_ast;          // распарсенная AST
    std::map<std::string, double> variables_map;  // текущие значения переменных
    double result = 0.0;             // вычисленное значение
    bool is_valid = true;            // валидна ли формула
    std::string error_message;       // сообщение об ошибке при парсинге/вычислении

    std::string author;              // кто создал формулу
    std::string created_at;          // ISO 8601 timestamp
    std::string modified_at;         // ISO 8601 timestamp

    // Вычисляет значение формулы при заданном контексте
    void evaluate(EvaluationContext& context);

    // Парсит и обновляет формулу из строки
    void set_formula(const std::string& text);

    // Устанавливает значение переменной
    void set_variable(const std::string& name, double value);

    // Получает все переменные, используемые в формуле
    std::vector<std::string> get_variables() const;
};

// JSON сериализация/десериализация формул (для хранения в базе)
std::string dimension_formula_to_json(const DimensionFormula& dim_formula);
DimensionFormula dimension_formula_from_json(const std::string& json_text);

}  // namespace bendcore::formula
