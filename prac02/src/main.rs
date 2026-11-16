use std::error::Error;
use std::io::{self, Write};

// 사용자에게 문자열을 입력받는 함수
fn prompt(message: &str) -> io::Result<String> {
    print!("{message}");
    io::stdout().flush()?;

    let mut line = String::new();

    if io::stdin().read_line(&mut line)? == 0 {
        return Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "입력 끝"
        ));
    }

    Ok(line)
}

// 행렬의 크기를 입력받는 함수
fn read_dimension(message: &str) -> io::Result<usize> {
    loop {
        let line = prompt(message)?;

        match line.trim().parse::<usize>() {
            Ok(value) if value > 0 => return Ok(value),

            _ => eprintln!("1 이상의 정수 입력"),
        }
    }
}

// 행렬의 원소를 입력받는 함수
fn read_matrix(
    name: &str,
    rows: usize,
    cols: usize
) -> Result<Vec<i64>, Box<dyn Error>> {

    let size = rows
        .checked_mul(cols)
        .ok_or("행렬 크기가 너무 큽니다.")?;

    let mut matrix = Vec::new();

    matrix.try_reserve_exact(size)?;

    println!(
        "{name} 행렬: 각 행마다 정수 {cols}개를 공백으로 구분하여 입력"
    );

    for row in 0..rows {
        loop {
            let line = prompt(
                &format!("{name}[{}] > ", row + 1)
            )?;

            let values: Result<Vec<i64>, _> =
                line.split_whitespace()
                    .map(str::parse::<i64>)
                    .collect();

            match values {
                Ok(values) if values.len() == cols => {
                    matrix.extend(values);
                    break;
                }

                _ => eprintln!(
                    "정수 {cols}개를 정확히 입력해 주세요."
                ),
            }
        }
    }

    Ok(matrix)
}

// 행렬 덧셈 수행
fn run() -> Result<(), Box<dyn Error>> {

    println!("Rust Vec<T>를 이용한 행렬 덧셈");

    let a_rows = read_dimension("A의 행 수: ")?;
    let a_cols = read_dimension("A의 열 수: ")?;

    let b_rows = read_dimension("B의 행 수: ")?;
    let b_cols = read_dimension("B의 열 수: ")?;

    // 두 행렬의 크기 비교
    if (a_rows, a_cols) != (b_rows, b_cols) {
        return Err(
            "두 행렬의 크기가 같아야 덧셈 가능".into()
        );
    }

    // 행렬 데이터 입력
    let a = read_matrix("A", a_rows, a_cols)?;
    let b = read_matrix("B", b_rows, b_cols)?;

    // 행렬 덧셈
    let result: Vec<i64> = a.iter()
        .zip(&b)
        .map(|(&x, &y)| {
            x.checked_add(y)
                .ok_or("정수 덧셈 오버플로 발생")
        })
        .collect::<Result<Vec<_>, _>>()?;

    // 결과 출력
    println!("\nA + B =");

    for row in result.chunks(a_cols) {
        for number in row {
            print!("{number:>8}");
        }

        println!();
    }

    Ok(())
}

// 메인 함수
fn main() {
    if let Err(error) = run() {
        eprintln!("오류: {error}");
        std::process::exit(1);
    }
}
