#!/usr/bin/env python
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# All rights reserved.
# Contact: Nokia Corporation (directui@nokia.com)
#
# This file is part of applauncherd.
#
# If you have questions regarding the use of this file, please contact
# Nokia at directui@nokia.com.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1 as published by the Free Software Foundation
# and appearing in the file LICENSE.LGPL included in the packaging
# of this file.

import unittest
from utils import *

class SecurityTests(unittest.TestCase):

    def setUp(self):
        if get_pid('applauncherd') == None:
            os.system('initctl start xsession/applauncherd')
        time.sleep(5)
        get_pid('booster-m')
        get_pid('booster-q')
        get_pid('booster-d')
        #setup here
        debug("Executing SetUp")

    def tearDown(self):
        #teardown here
        debug("Executing TearDown")
        if get_pid('applauncherd') == None:
            os.system('initctl start xsession/applauncherd')
        time.sleep(5)
        get_pid('booster-m')
        get_pid('booster-q')
        get_pid('booster-d')

    #Testcases
    def filter_creds(self, creds):
        """
        Filter out some unnecessary cruft from the test point of view
        """

        def f(x):
            return (x[:3] != "SRC" and
                    x[:3] != "AID" and
                    x != "applauncherd-testapps::applauncherd-testapps")

        return filter(f, creds)

    def user_creds(self, suppl = False):
        """
        Returns the user id, group id and optionally supplementary
        groups as credential tokens.
        """

        groups = []

        if suppl:
            groups = get_groups_for_user()

            def f(x):
                return 'GRP::' + x

            groups = map(f, groups)

        return ['UID::user', 'GID::users'] + groups

    def creds_defined_for_app(self, app):
        """
        Reads the security tokens an application should have from
        /sys/kernel/security/credp/policy and converts them to
        human-readable form with creds-test (from the package
        libcreds2-tools).
        """

        f = open("/sys/kernel/security/credp/policy")
        lines = f.readlines()
        f.close()

        creds = []
        found  = False

        for line in lines:
            if not found and line.find(app) != -1:
                found = True
                debug(line)
                continue

            if found:
                if re.match('^\s', line) != None:
                    creds.append(line.strip())
                    debug(line)
                else:
                    break

        def cred2str(cred):
            st, op = commands.getstatusoutput("creds-test %s" % cred)
            return op.split()[3]

        return [cred2str(cred) for cred in creds]
        
    def test_correct_creds(self):
        """
        Test that the fala_ft_creds* applications have the correct
        credentials set (check aegis file included in the debian package)
        """
        creds1 = launch_and_get_creds('/usr/bin/fala_ft_creds1')
        creds2 = launch_and_get_creds('/usr/bin/fala_ft_creds2')

        self.assert_(creds1 != None, "couldn't get credentials")
        self.assert_(creds2 != None, "couldn't get credentials")

        # When an application has a manifest, the users supplementary
        # groups are not by default included in the credential list,
        # only UID and GID.

        # required caps for fala_ft_creds1
        cap1 = self.creds_defined_for_app('fala_ft_creds1') + self.user_creds()

        # required caps for fala_ft_creds2
        cap2 = self.creds_defined_for_app('fala_ft_creds2') + self.user_creds()

        cap1.sort()
        cap2.sort()

        creds1.sort()
        creds2.sort()

        debug("fala_ft_creds1 has %s" % ', '.join(creds1))
        debug("fala_ft_creds1 requires %s" % ', '.join(cap1))

        debug("fala_ft_creds2 has %s" % ', '.join(creds2))
        debug("fala_ft_creds2 has %s" % ', '.join(cap2))

        self.assert_(cap1 == creds1, "fala_ft_creds1 has incorrect credentials")
        self.assert_(cap2 == creds2, "fala_ft_creds2 has incorrect credentials")

    def test_no_aegis(self):
        """
        Check that an application that doesn't have aegis file doesn't
        get any funny credentials.
        """

        creds = launch_and_get_creds('/usr/bin/fala_ft_hello')
        creds = self.filter_creds(creds)
        debug("fala_ft_hello has %s" % ', '.join(creds))

        self.assert_(creds != None, "error retrieving credentials")

        # Credentials should be dropped, but uid/gid + groups retained
        req_creds = self.user_creds(True)

        creds.sort()
        req_creds.sort()

        print "APP HAS:  " + ', '.join(creds)
        print "REQUIRED: " + ', '.join(req_creds)

        self.assert_(creds == req_creds,
                     "fala_ft_hello has incorrect credentials")

    def test_invoker_creds(self):
        """
        Test that the launcher registered customized credentials 
        and invoker has proper credentials to access launcher
        """

        INVOKER_BINARY='/usr/bin/invoker'
        FAKE_INVOKER_BINARY='/usr/bin/faulty_inv'
        
        #test application used for testing invoker
        Testapp = '/usr/bin/fala_ft_hello.launch'

        #launching the testapp with actual invoker
        debug("launching the testapp with actual invoker")
        
        st = os.system('%s --type=m --no-wait %s'%(INVOKER_BINARY, Testapp))
        pid = get_pid(Testapp.replace('.launch', ''))
        self.assert_((st == 0), "Application was not launched using launcher")
        self.assert_(not (pid == None), "Application was not launched using launcher: actual pid%s" %pid)

        kill_process(apppid=pid)  
        pid = get_pid(Testapp.replace('.launch', '')) 
        self.assert_((pid == None), "Application still running")        
        
        #launching the testapp with fake invoker
        debug("launching the testapp with fake invoker")
        st = os.system('%s --type=m --no-wait %s'%(FAKE_INVOKER_BINARY, Testapp)) 
        pid = get_pid(Testapp.replace('.launch', ''))
        self.assert_(not (st == 0), "Application was launched using fake launcher")
        self.assert_((pid == None), "Application was launched using fake launcher")

    def test_compare_application_invoker_creds(self):
        """
        Compare the credentials of invoker and the launched application
        when applauncherd is running and when it's not. Verify that
        the credentials are different.

        See NB#183566, NB#187583
        """

        def do_it():
            """
            Returns the credentials for both invoker and application
            that was launched.
            """

            # launch an application, leave invoker running
            print "launching application"
            invoker = run_app_as_user('/usr/bin/invoker --type=m --wait-term ' +
                                      'fala_ft_hello.launch')

            time.sleep(2)

            # get credentials
            invoker_creds = get_creds(path = 'invoker')
            app_creds = get_creds(path = 'fala_ft_hello')

            # when applauncherd is not running, invoker execs the app
            # so the credentials for invoker and app are the same
            if get_pid('invoker') == None and get_pid('fala_ft_hello') != None:
                invoker_creds = app_creds

            self.assert_(invoker_creds != None,
                         "error retrieving creds for invoker")
            self.assert_(app_creds != None,
                         "error retrieving creds for fala_ft_hello")

            invoker_creds = self.filter_creds(invoker_creds)
            app_creds = self.filter_creds(app_creds)

            invoker_creds.sort()
            app_creds.sort()

            print "invoker creds = %s" % invoker_creds
            print "app creds = %s" % app_creds

            kill_process('fala_ft_hello')

            return (invoker_creds, app_creds)

        # creds when applauncherd is running
        creds1 = do_it()

        stop_applauncherd()

        # creds when applauncherd *is not* running
        creds2 = do_it()

        start_applauncherd()

        # App shouldn't have the same credentials as invoker, when
        # applauncherd *is* running. Invoker has some credentials
        # to access applauncherd and naturally these shouldn't be
        # derived to the application
        self.assert_(creds1[0] != creds1[1],
                     'app creds are the same as invoker creds')

        # and the same when applauncherd is not running
        # note that the invoker doesn't show up in e.g. "ps ax"
        # because of its execing so we'll just use the creds from the
        # first step as they should be the same
        self.assert_(creds1[0] != creds2[1],
                     'app creds are the same as invoker creds when ' +
                     'applauncherd is not running')


    def test_compare_application_creds(self):
        """
        Launch an application as user and root both when applauncherd
        is running and when it isn't. Compare the credentials between
        the two cases and verify that they are the same.

        See NB#183566, NB#187583
        """

        def do_it():
            """
            A helper function to launch application and get credentials
            as user and root.
            """

            handle = Popen(['/usr/bin/fala_ft_hello'],
                           stdout = DEV_NULL, stderr = DEV_NULL)

            # give the application some time to launch up
            time.sleep(2)
            
            root = get_creds('fala_ft_hello')
            kill_process('fala_ft_hello')

            user = launch_and_get_creds('/usr/bin/fala_ft_hello')

            user.sort()
            root.sort()

            return (user, root)


        stop_applauncherd()

        # get creds for a launched application when applauncherd
        # is not running
        creds1 = do_it()

        start_applauncherd()

        # get creds for the same application when applauncherd
        # is running
        creds2 = do_it()

        # creds should be the same, regardless of applauncherd status
        self.assert_(creds1[0] == creds2[0], 'creds for user-case different')
        self.assert_(creds1[1] == creds2[1], 'creds for root-case different')

        # creds should be different, when run as user and when run as root,
        # regarless of applauncherd status
        self.assert_(creds1[0] != creds1[1],
                     'creds are same when applauncherd is not running')
        self.assert_(creds2[0] != creds2[1],
                     'creds are same when applauncherd is running')

    def test_dont_pass_invoker_creds(self):
        """
        Test that invoker specific credentials are not passed on to
        the launched application.
        """

        # credentials that invoker should have, but shouldn't be
        # passed to the application
        creds_to_filter = ['applauncherd-launcher::access',
                           'applauncherd-invoker::applauncherd-invoker']

        # launch an application with and without a manifest
        creds1 = launch_and_get_creds('/usr/bin/fala_ft_creds1')
        creds2 = launch_and_get_creds('/usr/bin/fala_ft_hello')

        # see that the above creds are not included
        for cred in creds_to_filter:
            self.assert_(cred not in creds1,
                         '%s was passed to fala_ft_creds1' % cred)

            self.assert_(cred not in creds2,
                         '%s was passed to fala_ft_hello' % cred)
        

# main
if __name__ == '__main__':
    # When run with testrunner, for some reason the PATH doesn't include
    # the tools/bin directory
    if os.getenv('_SBOX_DIR') != None:
        os.environ['PATH'] = os.getenv('PATH') + ":" + os.getenv('_SBOX_DIR') + '/tools/bin'
        using_scratchbox = True

    #check_prerequisites()
    #start_launcher_daemon()

    tests = sys.argv[1:]

    mysuite = unittest.TestSuite(map(SecurityTests, tests))
    result = unittest.TextTestRunner(verbosity=2).run(mysuite)

    if not result.wasSuccessful():
        sys.exit(1)

    sys.exit(0)
