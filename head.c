int main(int argc, char *argv[]) {
    /*
    head FILE
    head 
    head FILE1 FILE2 FILE3 .. 
    head -N
    head -N FILE
    head -N FILE1 FILE2 FILE3 ..
    head -n N 
    head -n N FILE
    head -n N FILE1 FILE2 FILE3 ..
    <commands> | head 
    */

    /*
    case 1: flag provided
        case 1.1: -N
            case 1.1.1: nothing after
                read from stdin
            case 1.1.2: single file
                read N lines from that file
            case 1.1.3: multiple files
                read N lines from those multiple files
        case 1.2: -n N
            case 1.2.1: nothing after
                read N lines from stdin
            case 1.2.2: single file
                read N lines from that one file
            case 1.2.3: multiple files
                read N lines from each of those files
    case 2: no flags
        case 2.1: nothing after
            read 10 (default) lines from stdin
        case 2.2: single file
            read 10 (default) lines from stdin
        case 2.3: multiple files
            read 10 (default) lines from each of those files
    */
}