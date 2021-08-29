/*
 * To build: ./build_ut.sh
 */
#include <iostream>
using namespace std;
#include <menaconlib.h>
#include <cpp4scripts.hpp>
using namespace c4s;

#include "Defines.hpp"
#include "SmtpMailer.hpp"
#include "SmtpMailer.cpp"

int main(int argc, char **argv)
{
    int ndx=0;
    SmtpMailer *mailer;
    SmtpMsg *msg;

    try {
        c4s::logbase::init_log(LL_TRACE, new c4s::stderr_sink());
        CS_PRINT_TRCE("t_mailer start");
        //mailer = new SmtpMailer("TestServer", "taina","pwd");
        mailer = new SmtpMailer("smtp://mail.menacon.fi:587", "info","xZ4okNRTDHJH");
        msg = new SmtpMsg("antti@menacon.fi", "Mailer test");

        msg->addRecipient("antti@menacon.fi");
        msg->body<<"Hi\n\n";
        msg->body<<"Aliquam tempus nisl ut mi aliquet, ut vulputate lacus venenatis. Nunc fringilla tempor purus. Mauris vel metus tristique, rutrum ligula eget, ullamcorper velit. Duis in leo ut eros imperdiet elementum. Quisque placerat non nulla vitae faucibus. Vivamus id tellus mi. Duis sollicitudin purus id purus varius, a dignissim elit luctus. Nulla non lacus gravida odio posuere hendrerit eu hendrerit sem. Nullam sagittis tempor est at rutrum. Duis vitae lorem vel ligula rhoncus sodales. Duis orci diam, ultrices vitae diam eget, tempor consectetur nunc. Maecenas vitae libero volutpat metus sollicitudin euismod.\n";
        msg->body<<"Vivamus vitae mi ut dolor egestas dictum quis consectetur mi. Suspendisse placerat nibh a neque hendrerit, at eleifend augue ullamcorper. Etiam viverra nisl in dictum placerat. Nunc cursus sapien a ante tristique, nec venenatis purus tempor. Cras ut sem tellus. Nullam vulputate dictum ipsum, vitae scelerisque nunc feugiat nec. Etiam a erat nec orci posuere congue. In lorem dolor, vehicula a mauris non, tempus cursus ipsum. Phasellus tristique eget leo quis imperdiet. Phasellus at nisl sit amet ipsum aliquet iaculis a ac ante. Integer mattis, neque vel fermentum cursus, nisi mi porttitor ante, vitae faucibus ante ex et nulla.\n\n";
        msg->body<<"--Menacon mailer\n";

        mailer->send(msg);
#ifdef UNIT_TEST
        while(mailer->perform_UT(msg) && ndx<20) {
            ndx++;
        }
#else
        while(mailer->perform() && ndx<20) {
            ndx++;
            sleep(1);
        }
#endif
        if(ndx>=20)
            cout<<"mailer perform: timeout\n.";
    }
    catch(const runtime_error &re) {
        cout<<"Mailer failed:"<<re.what()<<'\n';
    }
    cout<<"Done.\n";
    // delete msg; Mailer removes completed transfers!
    delete mailer;
    c4s::logbase::close_log();
    return 0;
}
