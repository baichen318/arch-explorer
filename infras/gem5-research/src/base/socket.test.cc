/*
 * Copyright (c) 2020 The Regents of the University of California
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtest/gtest.h>

#include "base/gtest/logging.hh"
#include "base/socket.hh"

static const int TestPort1 = 7893;
static const int TestPort2 = 7894;

using namespace gem5;

/*
 * Socket.test tests socket.cc. It should be noted that some features of
 * socket.cc have not been fully tested due to interaction with system-calls.
 */

class MockListenSocket : public ListenSocket
{
  public:
    /*
     * This mock Listen Socket is used to ensure the static variables are reset
     * back to their default values after deconstruction (i.e., after a test
     * has completed).
     */
    ~MockListenSocket()
    {
        cleanup();
    }
};

TEST(SocketTest, DefaultBehavior)
{
    /*
     * Tests the default behavior where listenSocket is constructed, and is
     * not listening to a port.
     */
    MockListenSocket listen_socket;
    EXPECT_EQ(-1, listen_socket.getfd());
    EXPECT_FALSE(listen_socket.islistening());
    EXPECT_FALSE(listen_socket.allDisabled());
}

TEST(SocketTest, DisableAll)
{
    MockListenSocket listen_socket;
    listen_socket.disableAll();
    EXPECT_EQ(-1, listen_socket.getfd());
    EXPECT_FALSE(listen_socket.islistening());
    EXPECT_TRUE(listen_socket.allDisabled());
}

TEST(SocketTest, ListenToPort)
{
    MockListenSocket listen_socket;
    EXPECT_TRUE(listen_socket.listen(TestPort1));
    EXPECT_NE(-1, listen_socket.getfd());
    EXPECT_TRUE(listen_socket.islistening());
    EXPECT_FALSE(listen_socket.allDisabled());
}

TEST(SocketTest, ListenToPortReuseFalse)
{
    MockListenSocket listen_socket;
    /*
     * The ListenSocket object should have the same state regardless as to
     * whether reuse is true or false (it is true by default).
     */
    EXPECT_TRUE(listen_socket.listen(TestPort1, false));
    EXPECT_NE(-1, listen_socket.getfd());
    EXPECT_TRUE(listen_socket.islistening());
    EXPECT_FALSE(listen_socket.allDisabled());
}

TEST(SocketTest, RelistenWithSameInstanceSamePort)
{
    MockListenSocket listen_socket;
    EXPECT_TRUE(listen_socket.listen(TestPort1));

    /*
     * You cannot listen to another port if you are already listening to one.
     */
    gtestLogOutput.str("");
    EXPECT_ANY_THROW(listen_socket.listen(TestPort1));
    std::string expected = "panic: Socket already listening!\n";
    std::string actual = gtestLogOutput.str();
    EXPECT_EQ(expected, actual);
}

TEST(SocketTest, RelistenWithSameInstanceDifferentPort)
{
    MockListenSocket listen_socket;
    EXPECT_TRUE(listen_socket.listen(TestPort1));

    /*
     * You cannot listen to another port if you are already listening to one.
     */
    gtestLogOutput.str("");
    EXPECT_ANY_THROW(listen_socket.listen(TestPort2));

    std::string expected = "panic: Socket already listening!\n";
    std::string actual = gtestLogOutput.str();
    EXPECT_EQ(expected, actual);
}

TEST(SocketTest, RelistenWithDifferentInstanceOnDifferentPort)
{
    MockListenSocket listen_socket;
    EXPECT_TRUE(listen_socket.listen(TestPort1));

    /*
     * You can listen to another port with a different instance.
     */
    MockListenSocket listen_socket_2;
    EXPECT_TRUE(listen_socket_2.listen(TestPort2));
}

TEST(SocketTest, RelistenWithDifferentInstanceOnSamePort)
{
    MockListenSocket listen_socket;
    EXPECT_TRUE(listen_socket.listen(TestPort1));

    /*
     * You cannot listen to a port that's already being listened to.
     */
    MockListenSocket listen_socket_2;
    EXPECT_FALSE(listen_socket_2.listen(TestPort1));
}

TEST(SocketTest, AcceptError)
{
    MockListenSocket listen_socket;
    EXPECT_EQ(-1, listen_socket.accept());
}
