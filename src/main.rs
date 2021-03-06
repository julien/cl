use rusqlite::{Connection, Error, Result};
use std::env;
use std::io;
use std::path;

const CONFIG_DIR: &str = ".config";
const NOTES_DB: &str = "notes.db";
const NOTE_MAX_DISPLAY_LENGTH: usize = 50;

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
            if let Err(e) = list() {
                eprintln!("Couldn't list notes: {:?}", e);
                std::process::exit(1);
            };
        }
        2 => {
            let cmd = &args[1].replace("-", "").to_lowercase();

            match &cmd[..] {
                "a" => {
                    if let Err(e) = add() {
                        eprintln!("Couldn't add note: {:?}", e);
                        std::process::exit(1);
                    };
                }
                "h" => usage(program_name),
                "l" => {
                    // XXX: Duplicated (see above)
                    if let Err(e) = list() {
                        eprintln!("Couldn't list notes: {:?}", e);
                        std::process::exit(1);
                    };
                }
                _ => usage(program_name),
            };
        }
        3 => {
            let cmd = &args[1].replace("-", "").to_lowercase();
            let id = args[2].parse::<u32>().unwrap_or(0);

            if id == 0 {
                usage(program_name);
                std::process::exit(1);
            }

            match &cmd[..] {
                "d" => {
                    if delete(id).is_err() {
                        eprintln!("Couldn't delete note.");
                        std::process::exit(1);
                    };
                }
                "m" => {
                    if mark(id).is_err() {
                        eprintln!("Couldn't mark note {} as completed.", id);
                        std::process::exit(1);
                    };
                }
                "v" => {
                    if view(id).is_err() {
                        eprintln!("Couldn't view note {}.", id);
                        std::process::exit(1);
                    };
                }
                _ => usage(program_name),
            };
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

// XXX: Shouldn't this return &str instead?
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
        Err(e) => Err(e),
        Ok(_) => match conn.close() {
            // XXX: Is this actually necessary?
            Ok(()) => Ok(()),
            Err((_, e)) => Err(e),
        },
    }
}

fn add() -> Result<(), Error> {
    println!("(hit ENTER to finish):");
    let mut buffer = String::new();
    if io::stdin().read_line(&mut buffer).is_err() {
        eprintln!("Couldn't read line");
        std::process::exit(1);
    };

    if let Err(e) = create_table() {
        return Err(e);
    };

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;

    conn.execute(
        "INSERT INTO notes(text) values(?);",
        [buffer.replace('\n', "")],
    )?;
    println!("Note added.");
    Ok(())
}

fn exists(id: u32) -> Result<usize, Error> {
    if let Err(e) = create_table() {
        return Err(e);
    };

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;
    let mut stmt = conn.prepare("SELECT count(*) FROM notes WHERE id = ?;")?;
    let iter = stmt.query_map([id], |row| Ok(Counter { total: row.get(0)? }))?;
    let results: Vec<Result<Counter, Error>> = iter.collect();
    let result = match results.get(0).unwrap() {
        Ok(v) => v.total,
        Err(_) => 0,
    };

    Ok(result)
}

fn delete(id: u32) -> Result<(), Error> {
    let result = match exists(id) {
        Err(e) => {
            return Err(e);
        }
        Ok(v) => v,
    };

    if result == 0 {
        eprintln!("Note {} not found.", id);
        std::process::exit(1);
    }

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;
    conn.execute("DELETE FROM notes WHERE id = ?;", [id])?;
    println!("Note deleted.");
    Ok(())
}

fn list() -> Result<usize, Error> {
    if let Err(e) = create_table() {
        return Err(e);
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
        eprintln!("No notes where found.");
        eprintln!("Try with the -h option for more information.");
    }

    let mut stmt = conn.prepare("SELECT * FROM notes;")?;
    let iter = stmt.query_map([], |row| {
        Ok(Note {
            id: row.get(0)?,
            text: row.get(1)?,
            done: row.get(2)?,
        })
    })?;

    for note in iter.flatten() {
        let mut text = note.text;
        if text.len() >= NOTE_MAX_DISPLAY_LENGTH {
            text.truncate(NOTE_MAX_DISPLAY_LENGTH - 3);
            text.push_str("...");
        }

        if let Some(1) = note.done {
            println!("\x1b[9m{} {}\x1b[0m ", note.id, text);
        } else {
            println!("{} {}", note.id, text);
        }
    }
    // XXX: Souldn't this return the list of notes?
    Ok(result)
}

fn mark(id: u32) -> Result<(), Error> {
    let result = match exists(id) {
        Err(e) => {
            return Err(e);
        }
        Ok(v) => v,
    };

    if result == 0 {
        eprintln!("Note {} not found.", id);
        std::process::exit(1);
    }

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;
    conn.execute("UPDATE notes SET done = 1 WHERE id = ?;", [id])?;
    println!("Note {} marked as completed.", id);
    Ok(())
}

fn view(id: u32) -> Result<(), Error> {
    let result = match exists(id) {
        Err(e) => {
            return Err(e);
        }
        Ok(v) => v,
    };

    if result == 0 {
        eprintln!("Note {} not found.", id);
        std::process::exit(1);
    }

    let db_path = get_db_path();
    let conn = Connection::open(db_path)?;
    let mut stmt = conn.prepare("SELECT id,text,done FROM notes WHERE id = ?;")?;
    let iter = stmt.query_map([id], |row| {
        Ok(Note {
            id: row.get(0)?,
            text: row.get(1)?,
            done: row.get(2)?,
        })
    })?;

    for note in iter.flatten() {
        println!("{}", note.text);
    }
    Ok(())
}
