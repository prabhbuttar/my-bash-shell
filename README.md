# my-bash-shell
mbash25 - A Custom Shell in C
mbash25 is a custom Unix shell written in C that replicates and extends the functionality of a standard shell like bash. It provides a command-line interface for users to interact with the operating system, supporting command execution, piping, I/O redirection, background processes, and several custom operators.

Features
This shell supports a variety of features common to Unix-like operating systems:

Command Execution: Executes standard Unix commands with arguments (e.g., ls -l, pwd).

Piping (|): Chains multiple commands together, where the output of one command becomes the input of the next.

Example: ls -l | wc -l

I/O Redirection:

Input (<): Redirects the input of a command to come from a file.

Example: sort < file.txt

Output (>): Redirects the output of a command to a file, overwriting the file if it exists.

Example: ls > files.txt

Append Output (>>): Redirects the output of a command to a file, appending to the end of the file.

Example: echo "Hello" >> log.txt

Sequential Execution (;): Executes multiple commands in sequence, one after the other.

Example: pwd ; ls

Background Processes (&): Runs commands in the background, allowing the user to continue using the shell.

Example: gedit &

Foreground Process (fg): Brings the most recent background process to the foreground.

Conditional Execution:

AND (&&): Executes the second command only if the first command succeeds.

Example: mkdir temp && cd temp

OR (||): Executes the second command only if the first command fails.

Example: cat non_existent_file.txt || echo "File not found"

New Terminal (newt): Opens a new xterm window running an instance of this shell.

Exit Shell (killterm): Terminates the current shell session.

Custom Operators

mbash25 also introduces several unique operators:

Word Count (#): Counts the words in one to four specified files using wc -w.

Usage: # file1.txt file2.txt

File Concatenation (++): Concatenates two to five files and prints the result to standard output.

Usage: ++ file1.txt file2.txt file3.txt

Mutual File Append (+): Appends the content of the second file to the first, and then appends the newly modified first file to the second.

Usage: file1.txt + file2.txt

Reversed Pipe (~): A reverse pipe that executes a chain of commands from right to left.

Usage: cmd3 ~ cmd2 ~ cmd1 (executes as cmd1 | cmd2 | cmd3)

Getting Started
Prerequisites

A C compiler (like gcc)

A Unix-like operating system (Linux, macOS)

Compilation

To compile the shell, use the following command in your terminal:

gcc -o mbash25 mbash25_Prabhdeep_Singh_110189332.c

This will create an executable file named mbash25.

Running the Shell

To start the shell, run the compiled executable from your terminal:

./mbash25

You will see the mbash25$ prompt, and you can start entering commands.

Author
Prabhdeep Singh

