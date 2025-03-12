

Testing Strategy:
The test plan consisted of both interactive and batch mode testing:
Interactive Testing: testing various commands directly to the shell program (mysh), checking input handling, edge case handling, and many more.

Batch Mode Testing: Running batch scripts with different conditions. including large files and files with various formatting and special characters.

Test Cases:

Basic Case:
- This basic test case verifies basic directory navigation, file creation, and command functionality. 
- It tests commands like mkdir, pwd, cd, ls, which. 

Redirect Case:
- This case called RedirectCase.sh would create a directory, change directory and would test if file redirection would work. 
- It incorporated many redirect input and output cases, where it would create a new file if not already made and write the output to them.

Wilcard Case: 
- This case called wildcardCase.sh, runs tests to ensure that wildcards work along with a directory. It handles file pattern matching using wildcard (*). It tests listing .txt files, then checks for files start with test.
- It also counts the files matching the pattern report_*.csv., Validating whether the shell correctly matches files with specific extensions or patterns. 

Pipe Case:
- This case called pipeCase.sh ensures that pipes are working correctly.
- Handling pipe test with grep, word count, and multiple pipes. It also changes directory and reads the inputs of the file 'f1' to run tests on.
- create the pipe_tests and make a file where each line is a different letter.

