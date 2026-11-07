#!/usr/bin/env bash
# Проверка правила «никаких заглушек» (docs/PROJECT_PROCESS.md, раздел 2).
#
# Режимы:
#   scripts/check_no_stubs.sh                  полная проверка каталогов нового кода
#   scripts/check_no_stubs.sh --diff <ref>     то же + только добавленные строки в файлах,
#                                              изменённых относительно merge-base(<ref>, HEAD)
#                                              (включая незакоммиченные изменения рабочей копии)
# Опции:
#   --root <dir>      корень репозитория (по умолчанию — git-корень каталога скрипта)
#   --paths <p>...    проверять эти файлы/каталоги вместо каталогов по умолчанию
#                     (относительно --root; до следующей опции или конца строки)
#   --no-full         не делать полную проверку каталогов (только --diff)
#   --list-rules      вывести правила и выйти
#
# Вывод: <файл>:<строка>: [<правило>] <пояснение> | <фрагмент строки>
# Код выхода: 0 — нарушений нет, 1 — найдены нарушения, 2 — ошибка запуска.
#
# Подавление ложного срабатывания (только с причиной, см. docs/qa/QA.md):
#   на той же или предыдущей строке комментарий  qa-allow(<правило>): <причина, >= 12 символов>
set -euo pipefail

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; }

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root=""
diff_ref=""
full=1
list_rules=0
paths=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --diff)
      [[ $# -ge 2 ]] || { echo "check_no_stubs: --diff требует ссылку git" >&2; exit 2; }
      diff_ref="$2"; shift 2 ;;
    --root)
      [[ $# -ge 2 ]] || { echo "check_no_stubs: --root требует каталог" >&2; exit 2; }
      root="$2"; shift 2 ;;
    --paths)
      shift
      while [[ $# -gt 0 && "$1" != --* ]]; do paths+=("$1"); shift; done
      [[ ${#paths[@]} -gt 0 ]] || { echo "check_no_stubs: --paths требует хотя бы один путь" >&2; exit 2; } ;;
    --no-full) full=0; shift ;;
    --list-rules) list_rules=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "check_no_stubs: неизвестный аргумент: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ -z "$root" ]]; then
  root="$(git -C "$script_dir" rev-parse --show-toplevel 2>/dev/null || (cd "$script_dir/.." && pwd))"
fi
root="$(cd "$root" && pwd)" || { echo "check_no_stubs: нет каталога $root" >&2; exit 2; }

if ! command -v perl >/dev/null 2>&1; then
  echo "check_no_stubs: требуется perl" >&2; exit 2
fi

# Каталоги нового кода (docs/ROADMAP_GENERAL_CAD.md, фаза 0); отсутствующие пропускаются.
default_dirs=(
  src/kernel src/modeling src/assembly_v2 src/drawing_v2
  include/bendcore/kernel include/bendcore/modeling include/bendcore/assembly_v2 include/bendcore/drawing_v2
  tests/kernel tests/modeling tests/assembly_v2 tests/drawing_v2
)
[[ ${#paths[@]} -gt 0 ]] || paths=("${default_dirs[@]}")

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
: >"$tmp/full.lst"
: >"$tmp/diff.txt"

# Файлы с кодом. Исключения: фикстуры самотеста (содержат нарушения намеренно),
# сам проверяющий скрипт и его самотест (содержат шаблоны правил), сторонний код.
is_code_file() {
  case "$1" in
    tests/qa/fixtures/*|*/tests/qa/fixtures/*) return 1 ;;
    third_party/*|third_party_stubs/*|node_modules/*|*/node_modules/*) return 1 ;;
    scripts/check_no_stubs.sh|tests/qa/test_check_no_stubs.sh) return 1 ;;
  esac
  case "$1" in
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.ipp|*.inl|*.tpp) return 0 ;;
    *.cmake|*/CMakeLists.txt|CMakeLists.txt) return 0 ;;
    *.sh|*.bash|*.py|*.js|*.mjs|*.cjs|*.ts|*.tsx|*.jsx) return 0 ;;
  esac
  return 1
}

if [[ $full -eq 1 ]]; then
  for p in "${paths[@]}"; do
    if [[ -d "$root/$p" ]]; then
      while IFS= read -r -d '' f; do
        rel="${f#"$root"/}"
        # Явно указанный путь проверяется целиком (самотест передаёт фикстуры через --paths).
        if [[ "$p" == tests/qa/fixtures* ]]; then echo "$rel" >>"$tmp/full.lst"
        elif is_code_file "$rel"; then echo "$rel" >>"$tmp/full.lst"; fi
      done < <(find "$root/$p" -type f -print0 | sort -z)
    elif [[ -f "$root/$p" ]]; then
      echo "$p" >>"$tmp/full.lst"
    fi
  done
fi

if [[ -n "$diff_ref" ]]; then
  if ! git -C "$root" rev-parse --verify --quiet "${diff_ref}^{commit}" >/dev/null; then
    echo "check_no_stubs: ссылка '$diff_ref' не найдена в $root (нужна полная история: fetch-depth: 0)" >&2
    exit 2
  fi
  base="$(git -C "$root" merge-base "$diff_ref" HEAD)" || {
    echo "check_no_stubs: нет общего предка '$diff_ref' и HEAD" >&2; exit 2; }
  git -C "$root" diff --no-color --no-ext-diff --unified=0 -M --diff-filter=AMR "$base" >"$tmp/diff.txt"
fi

export CNS_ROOT="$root" CNS_FULL_LIST="$tmp/full.lst" CNS_DIFF="$tmp/diff.txt" CNS_LIST_RULES="$list_rules"
export CNS_DIFF_REF="$diff_ref"

# Список файлов из diff фильтруется той же функцией is_code_file.
if [[ -s "$tmp/diff.txt" ]]; then
  grep -E '^\+\+\+ b/' "$tmp/diff.txt" | sed 's#^+++ b/##' | while IFS= read -r f; do
    if is_code_file "$f"; then echo "$f"; fi
  done >"$tmp/diff_files.lst"
else
  : >"$tmp/diff_files.lst"
fi
export CNS_DIFF_FILES="$tmp/diff_files.lst"

exec perl -x "$0"
exit 2

#!perl
use strict;
use warnings;
use utf8;

my $root = $ENV{CNS_ROOT};
binmode(STDOUT, ':encoding(UTF-8)');
binmode(STDERR, ':encoding(UTF-8)');

# ---------------------------------------------------------------- правила по строкам
# id, пояснение, регулярное выражение, область (all | nontest | test)
my @line_rules = (
  ['todo', 'маркер незавершённой работы (TODO/FIXME/XXX/HACK)',
   qr/\b(?:TODO|FIXME|XXX|HACK)\b/, 'all'],
  ['placeholder', '«placeholder» — описание отсутствующей логики',
   qr/(?<!::)\bplace[- ]?holders?\b(?!\s*=\s*["'{]|\s*:\s*["'`])/i, 'all'],
  ['real-implementation', 'ссылка на «настоящую реализацию» вместо неё самой',
   qr/\bin\s+(?:a|the)\s+(?:real|full|production|complete|proper|final)\s+(?:implementation|version|system|code|solver)\b|\b(?:real|full|proper|production|complete)\s+implementation\s+(?:would|should|will|must|needs?)\b/i, 'all'],
  ['would', '«would ...» — описание того, что код должен был бы делать',
   qr/\bwould\s+(?:normally|actually|typically|ideally|eventually|usually|go\s+here)\b|\b(?:real|full|production|proper|complete)\s+(?:code|implementation|version|system|solver|algorithm)\s+would\b|\bwe\s+would\b|\bwould\s+be\s+(?:implemented|computed|calculated|handled|added|done|called|parsed|loaded|used|performed|replaced)\s+(?:here|later)\b|\bthis\s+would\s+(?:be|need|require|compute|call|use|perform)\s+(?:a|an|the|real|proper|actual)\b/i, 'all'],
  ['stub', '«stub» — заглушка',
   qr/(?<![a-z])(?<!no_)(?<!no-)stub(?:s|bed|by)?(?![a-z])/i, 'all'],
  ['not-implemented', '«not implemented» — объявленная, но не реализованная функциональность',
   qr/\bnot\s+(?:yet\s+)?implemented\b|\bunimplemented\b|\bNotImplemented\b|\bnot\s+supported\s+yet\b|\bcoming\s+soon\b/i, 'all'],
  ['simplified', '«simplified» как описание упрощённой/отсутствующей логики',
   qr/\b(?:simplified|simplistic|naive)\s+(?:version|implementation|approach|logic|calculation|computation|check|algorithm|model|estimate|stand-?in|formula)\b|(?:\/\/|#|\/\*|^\s*\*)\s*simplified\b|\bsimplified\s*(?::|\(|for\s+now)/i, 'all'],
  ['temporary', '«for now / temporary» — временное решение вместо логики',
   qr/\bfor\s+(?:now|the\s+time\s+being|simplicity)\b|\btemporar(?:y|ily)\s+(?:hack|solution|workaround|implementation|value)\b/i, 'all'],
  ['fake-value', 'фиктивное/зашитое значение вместо вычисления',
   qr/\b(?:dummy|fake|hard-?coded|mock(?:ed)?|canned|arbitrary)\s+(?:value|values|result|results|data|implementation|response|output|return|number)\b|\breturn\b[^;]*;\s*(?:\/\/|\/\*).*\b(?:assume[sd]?|dummy|fake|hard-?coded|for\s+now|pretend)\b/i, 'nontest'],
  ['trivial-assert', 'проверка, которая не может упасть',
   qr/\b(?:EXPECT|ASSERT)_TRUE\s*\(\s*(?:true|1)\s*\)|\b(?:EXPECT|ASSERT)_FALSE\s*\(\s*(?:false|0)\s*\)|\bassert\s*\(\s*(?:true|1|!0)\s*\)|\bSUCCEED\s*\(\s*\)|\b(?:EXPECT|ASSERT)_(?:EQ|NE|NEAR|DOUBLE_EQ|FLOAT_EQ|LE|GE)\s*\(\s*([A-Za-z_][\w.:>-]*)\s*,\s*\1\s*[,)]/, 'all'],
);

my %rule_ids = map { $_->[0] => 1 } @line_rules;
$rule_ids{$_} = 1 for qw(empty-body literal-return literal-success no-assertions);

if ($ENV{CNS_LIST_RULES}) {
  printf "%-20s %s\n", $_->[0], $_->[1] for @line_rules;
  printf "%-20s %s\n", 'empty-body', 'пустое тело функции у объявленной функциональности';
  printf "%-20s %s\n", 'literal-return', 'функция с параметрами возвращает только литерал';
  printf "%-20s %s\n", 'literal-success', 'success/ok = true до какого-либо вычисления';
  printf "%-20s %s\n", 'no-assertions', 'тестовый файл без единой проверки';
  printf "%-20s %s\n", 'bad-suppression', 'qa-allow без причины или с неизвестным правилом';
  exit 0;
}

sub read_list {
  my ($path) = @_;
  my @r;
  open(my $fh, '<', $path) or return @r;
  while (<$fh>) { chomp; push @r, $_ if length; }
  close $fh;
  return @r;
}

# ---------------------------------------------------------------- набор строк для проверки
# %targets: файл => undef (проверять все строки) | { номер_строки => 1 } (только добавленные)
my %targets;
$targets{$_} = undef for read_list($ENV{CNS_FULL_LIST});
my %diff_allowed = map { $_ => 1 } read_list($ENV{CNS_DIFF_FILES});
my %diff_added_file;   # файлы, добавленные целиком (для no-assertions)

if (-s $ENV{CNS_DIFF}) {
  open(my $dh, '<', $ENV{CNS_DIFF}) or die "check_no_stubs: не открыть diff: $!\n";
  my ($cur, $line, $is_new) = (undef, 0, 0);
  while (my $l = <$dh>) {
    if ($l =~ /^diff --git /) { $cur = undef; $is_new = 0; next; }
    if ($l =~ /^new file mode/) { $is_new = 1; next; }
    if ($l =~ /^\+\+\+ (?:b\/(.*)|\/dev\/null)$/) {
      $cur = defined $1 ? $1 : undef;
      $cur = undef if defined $cur && !$diff_allowed{$cur};
      $diff_added_file{$cur} = 1 if defined $cur && $is_new;
      next;
    }
    next unless defined $cur;
    if ($l =~ /^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@/) { $line = $1; next; }
    if ($l =~ /^\+/) {
      if (!exists $targets{$cur} || defined $targets{$cur}) {
        $targets{$cur} //= {};
        $targets{$cur}{$line} = 1;
      }
      $line++;
    }
  }
  close $dh;
}

# ---------------------------------------------------------------- вспомогательные функции
my @violations;
my $is_test_path = qr/(?:^|\/)tests?\//;

sub suppressed {
  my ($lines, $idx, $rule) = @_;   # $idx — индекс с 0
  for my $j ($idx, $idx - 1) {
    next if $j < 0;
    my $t = $lines->[$j];   # копия: pos() у /g не должен оставаться на элементе массива
    while ($t =~ /qa-allow\(([a-z-]+)\)\s*:\s*(.*)$/g) {
      my ($r, $why) = ($1, $2);
      $why =~ s/\s*(?:\*\/|-->)\s*$//;
      return 1 if $r eq $rule && length($why) >= 12;
    }
  }
  return 0;
}

sub report {
  my ($file, $lines, $idx, $rule, $msg, $want) = @_;
  return if defined $want && !$want->{$idx + 1};
  return if suppressed($lines, $idx, $rule);
  my $snip = $lines->[$idx] // '';
  $snip =~ s/^\s+|\s+$//g;
  utf8::decode($snip);
  $snip = substr($snip, 0, 120) . '...' if length($snip) > 123;
  push @violations, "$file:" . ($idx + 1) . ": [$rule] $msg | $snip";
}

my %cpp_kw = map { $_ => 1 } qw(if for while switch catch return sizeof alignof decltype
  static_assert defined do else new delete throw case operator noexcept requires);

sub line_of_offset {
  my ($text, $off) = @_;
  return (substr($text, 0, $off) =~ tr/\n//);
}

# ---------------------------------------------------------------- проверка файла
for my $file (sort keys %targets) {
  my $want = $targets{$file};
  my $path = "$root/$file";
  next unless -f $path;
  open(my $fh, '<', $path) or do { warn "check_no_stubs: не открыть $file: $!\n"; next; };
  my $text = do { local $/; <$fh> };
  close $fh;
  next if $text =~ /\x00/;   # двоичный файл
  my @lines = split /\n/, $text, -1;
  my $is_test = $file =~ $is_test_path;
  my $is_cpp = $file =~ /\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|ipp|inl|tpp)$/;

  for my $i (0 .. $#lines) {
    next if defined $want && !$want->{$i + 1};
    my $l = $lines[$i];
    for my $r (@line_rules) {
      my ($id, $msg, $re, $scope) = @$r;
      next if $scope eq 'nontest' && $is_test;
      next if $scope eq 'test' && !$is_test;
      report($file, \@lines, $i, $id, $msg, $want) if $l =~ $re;
    }
    while ($l =~ /qa-allow\(([^)]*)\)(\s*:\s*(.*))?/g) {
      my ($r, $why) = ($1, defined $3 ? $3 : '');
      $why =~ s/\s*(?:\*\/|-->)\s*$//;
      if (!$rule_ids{$r}) {
        push @violations, "$file:" . ($i + 1) . ": [bad-suppression] неизвестное правило '$r' в qa-allow";
      } elsif (length($why) < 12) {
        push @violations, "$file:" . ($i + 1) . ": [bad-suppression] qa-allow($r) без причины (нужно >= 12 символов после ':')";
      }
    }
  }

  next unless $is_cpp;

  # Тексты без комментариев и строковых литералов (длина и переводы строк сохраняются),
  # чтобы многострочные эвристики не срабатывали на код в комментариях.
  my $code = $text;
  $code =~ s{(/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')}{
    my $m = $1; (my $k = $m) =~ s/[^\n]/ /g;
    ($m =~ /^"/) ? '"' . substr($k, 1, -1) . '"' : ($m =~ /^'/ ? "'" . substr($k, 1, -1) . "'" : $k)
  }gse;

  # Последнее имя class/struct перед позицией — для распознавания конструкторов.
  my @classes;
  while ($code =~ /\b(?:class|struct)\s+(?:alignas\s*\([^)]*\)\s*)?(\w+)/g) {
    push @classes, [pos($code), $1];
  }
  my $class_before = sub {
    my ($off) = @_;
    my $name = '';
    for my $c (@classes) { last if $c->[0] > $off; $name = $c->[1]; }
    return $name;
  };
  my $qual = qr/(?:\s*(?:const|noexcept(?:\s*\([^)]*\))?|override|final|volatile|&&?))*/;

  # empty-body / literal-return — только для кода продукта: тестовые двойники (fake/mock)
  # в тестах законно возвращают константы.
  my $nontest = !$is_test;

  # empty-body: функция с пустым телом.
  while ($code =~ /(~?)\b(\w+)\s*\(([^()]*)\)$qual\s*\{\s*\}/g) {
    my ($tilde, $name, $start) = ($1, $2, $-[0]);
    next if !$nontest || $tilde || $cpp_kw{$name};
    my $before = substr($code, 0, $start);
    next if $before =~ /(?:[:,]|\boperator\s*\S*|\]\s*|=\s*|\breturn\s*|\(\s*)\s*$/;   # список инициализации, лямбда, выражение
    next if $before =~ /\b\Q$name\E\s*::\s*$/;   # A::A() {}
    next if $name eq $class_before->($start);    # конструктор внутри класса
    next if $before =~ /\b(?:struct|class|union|enum|namespace)\s+[\w:]*\s*$/;
    my $ni = line_of_offset($code, $start);
    report($file, \@lines, $ni, 'empty-body', "пустое тело функции '$name'", $want);
  }

  # literal-return: функция с параметрами, тело которой — только return литерала.
  my $lit = qr/(?:true|false|-?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?[fFuUlL]*|nullptr|\{\s*\}|""|std::nullopt|std::string\(\))/;
  while ($code =~ /\b(\w+)\s*\(([^()]*)\)$qual\s*(?:->\s*[\w:<>]+\s*)?\{\s*return\s+$lit\s*;\s*\}/g) {
    my ($name, $params, $start) = ($1, $2, $-[0]);
    next if !$nontest || $cpp_kw{$name};
    (my $p = $params) =~ s/\s+//g;
    next if $p eq '' || $p eq 'void';
    my $before = substr($code, 0, $start);
    next if $before =~ /(?:[:,=(]|\]|\breturn)\s*$/;
    my $ri = line_of_offset($code, index($code, 'return', $start));
    report($file, \@lines, $ri, 'literal-return',
           "функция '$name' с параметрами возвращает только литерал", $want);
  }

  # literal-success: success/ok/valid = true до любого вычисления в теле функции.
  my @clines = split /\n/, $code, -1;
  for my $i (0 .. $#clines) {
    next if defined $want && !$want->{$i + 1};
    next unless $clines[$i] =~ /^\s*(?:[\w\]\[]+(?:\.|->))*(?:success|succeeded|ok|is_ok|valid|is_valid|passed|converged)\s*=\s*true\s*;/;
    my $flag = 0;
    for (my $j = $i - 1; $j >= 0 && $j >= $i - 12; --$j) {
      my $c = $clines[$j];
      next if $c =~ /^\s*$/;
      if ($c =~ /\)\s*(?:const|noexcept|override|final|\s)*(?:->\s*[\w:<>]+\s*)?\{\s*$/ &&
          $c !~ /^\s*(?:\}\s*)?(?:if|else|for|while|switch|catch|do)\b/ && $c !~ /\]\s*\(/) {
        # Имя функции: перед скобкой, парной последней ')' заголовка.
        my $hdr = join("\n", @clines[($j >= 6 ? $j - 6 : 0) .. $j]);
        my $end = rindex($hdr, ')');
        my ($depth, $k) = (0, $end);
        for (; $k >= 0; --$k) {
          my $ch = substr($hdr, $k, 1);
          $depth++ if $ch eq ')';
          $depth-- if $ch eq '(';
          last if $depth == 0;
        }
        my ($fname) = $k > 0 ? (substr($hdr, 0, $k) =~ /(\w+)\s*$/) : ('');
        $fname //= '';
        # Фабрики «успешного результата» (success_result, make_ok, Ok) — не заглушки.
        $flag = 1 unless $fname =~ /succe|(?:^|_)ok(?:_|$)|^Ok$|^ok$|^make_ok/i;
        last;
      }
      last if $c =~ /[()]/ || $c =~ /\b(?:if|else|for|while|switch|return|case)\b/ || $c =~ /[{}]/;
    }
    # Инициализация «успех, пока проверка не провалится» допустима: срабатываем, только если
    # после флага функция возвращается, ничего не вычислив (строки без вызовов и ветвлений).
    if ($flag) {
      $flag = 0;
      for (my $j = $i + 1; $j <= $#clines && $j <= $i + 12; ++$j) {
        my $c = $clines[$j];
        next if $c =~ /^\s*$/;
        if ($c =~ /^\s*return\b[^()]*;/ || $c =~ /^\s*\}\s*$/) { $flag = 1; last; }
        last if $c =~ /[(){}]/ || $c =~ /\b(?:if|else|for|while|switch|case)\b/;
      }
    }
    report($file, \@lines, $i, 'literal-success', 'флаг успеха выставлен без вычисления', $want) if $flag;
  }

  # no-assertions: тестовый файл (main или TEST) без единой проверки.
  if ($is_test && $file =~ /\.(?:c|cc|cpp|cxx)$/ && (!defined $want || $diff_added_file{$file})) {
    if ($code =~ /\bint\s+main\s*\(|\bTEST(?:_F|_P)?\s*\(/ &&
        $code !~ /\b(?:assert|static_assert|EXPECT_\w+|ASSERT_\w+|CHECK\w*|REQUIRE\w*|check\w*|expect\w*|fail\w*|abort|EXIT_FAILURE)\s*\(|\bEXIT_FAILURE\b|\breturn\s+[1-9]\s*;|\bthrow\b/) {
      push @violations, "$file:1: [no-assertions] тестовый файл без единой проверки"
        unless suppressed(\@lines, 0, 'no-assertions') || suppressed(\@lines, 1, 'no-assertions');
    }
  }
}

my %seen;
@violations = grep { !$seen{$_}++ } @violations;
# Порядок: файл, строка, правило.
@violations = map { $_->[3] } sort { $a->[0] cmp $b->[0] || $a->[1] <=> $b->[1] || $a->[2] cmp $b->[2] }
  map { /^(.*?):(\d+): \[([a-z-]+)\]/ ? [$1, $2, $3, $_] : ['', 0, '', $_] } @violations;
my $nfiles = scalar keys %targets;
if (@violations) {
  print "$_\n" for @violations;
  printf STDERR "check_no_stubs: нарушений: %d (файлов проверено: %d). Правила: docs/PROJECT_PROCESS.md, раздел 2; подавление: docs/qa/QA.md\n",
    scalar(@violations), $nfiles;
  exit 1;
}
printf STDERR "check_no_stubs: нарушений нет (файлов проверено: %d%s)\n", $nfiles,
  ($ENV{CNS_DIFF_REF} ? ", diff против $ENV{CNS_DIFF_REF}" : '');
exit 0;
