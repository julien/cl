#[allow(unused_imports)]
use rusqlite::{AndThenRows, Connection, Error, MappedRows, Result};
use std::env;
use std::path;

const CONFIG_DIR: &str = ".config";
const NOTES_DB: &str = "notes.db";

#[derive(Debug)]
struct Counter {
    total: usize,
}

#[derive(Debug)]
struct Note {
    id: u32,
    text: String,
    done: Option<u32>,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let program_name = &args[0];
    match args.len() {
        1 => {
            match list() {
                Err(e) => {
                    println!("Couldn't list notes: {:?}", e);
                    std::process::exit(1);
                }
                _ => (),
            };
        }
        2 => {
            let cmd = &args[1].replace("-", "").to_lowercase();

            match &cmd.to_lowercase()[..] {
                "a" => {
                    println!("You want to add a note");
                }
                "h" => usage(program_name),
                "l" => {
                    println!("You want to list notes");
                }
                _ => usage(program_name),
            };
        }
        3 => {
            let cmd = &args[1];
            let id = &args[2];

            println!("cmd {}, id {}", cmd, id);
            // TODO: Check args ...
        }
        _ => usage(program_name),
    }
}

fn usage(program_name: &str) {
    println!("Usage: {} [OPTION]... [ARGUMENT]...", program_name);
    println!("Simple note taking");
    println!("Options:");
    println!("  -a     add a new note");
    println!("  -d id  delete note (specified by id)");
    println!("  -e id  edit note (specified by id)");
    println!("  -h     print this message");
    println!("  -l     list all notes");
    println!("  -m id  mark note (specified by id) as completed");
    println!("  -v id  view note (specified by id)");
    println!("\nIf no options are provided, the notes will be listed.");
}

fn list() -> Result<usize, Error> {
    match create_table() {
        Err(e) => {
            return Err(e);
        }
        _ => (),
    };

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;

    let mut stmt = conn.prepare("SELECT count(*) FROM notes;")?;
    let iter = stmt.query_map([], |row| Ok(Counter { total: row.get(0)? }))?;

    let results: Vec<Result<Counter, Error>> = iter.collect();
    let result = match results.get(0).unwrap() {
        Ok(v) => v.total,
        Err(_) => 0,
    };

    if result == 0 {
        println!("No notes where found.");
        println!("Try with the -h option for more information.");
    }

    let mut stmt = conn.prepare("SELECT * FROM notes;")?;
    let iter = stmt.query_map([], |row| {
        Ok(Note {
            id: row.get(0)?,
            text: row.get(1)?,
            done: row.get(2)?,
        })
    })?;

    for note in iter {
        // TODO: Display notes correctly
        println!("{:?}", note);
    }

    Ok(result)
}

fn get_db_path() -> String {
    let home_dir = match env::var("HOME") {
        Ok(val) => val,
        Err(_) => String::from(""),
    };

    let data_dir = match env::var("XDG_DATA_DIR") {
        Ok(val) => val,
        Err(_) => String::from(CONFIG_DIR),
    };

    let sep = path::MAIN_SEPARATOR.to_string();

    let mut db_path = String::new();
    db_path.push_str(&home_dir);
    db_path.push_str(&sep);
    db_path.push_str(&data_dir);
    db_path.push_str(&sep);
    db_path.push_str(NOTES_DB);

    db_path
}

fn create_table() -> Result<(), rusqlite::Error> {
    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;

    match conn.execute(
        "CREATE TABLE IF NOT EXISTS notes(
            id INTEGER PRIMARY KEY AUTOINCREMENT, text TEXT, done INT);",
        [],
    ) {
        Err(e) => {
            return Err(e);
        }
        Ok(_) => match conn.close() {
            Ok(()) => Ok(()),
            Err((_, e)) => Err(e),
        },
    }
}
