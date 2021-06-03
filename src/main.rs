use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    let program_name = &args[0];
    match args.len() {
        1 => {
            // TODO: List notes since no arguments were passed
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
    println!("Usage: {} [OPTION]... [ARGUMENT]...\n", program_name);
    println!("Simple note taking\n\n");
    println!("Options:\n");
    println!("  -a     add a new note\n");
    println!("  -d id  delete note (specified by id)\n");
    println!("  -e id  edit note (specified by id)\n");
    println!("  -h     print this message\n");
    println!("  -l     list all notes\n");
    println!("  -m id  mark note (specified by id) as completed\n");
    println!("  -v id  view note (specified by id)\n");
    println!("\nIf no options are provided, the notes will be listed.\n");
}
