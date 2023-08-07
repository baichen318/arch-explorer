/*
 * Copyright (c) 2015, University of Kaiserslautern
 * Copyright (c) 2016, Dresden University of Technology (TU Dresden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SYSTEMC_TLM_BRIDGE_SC_EXT_HH__
#define __SYSTEMC_TLM_BRIDGE_SC_EXT_HH__

#include <memory>

#include "base/amo.hh"
#include "mem/packet.hh"
#include "systemc/ext/tlm_core/2/generic_payload/gp.hh"

namespace Gem5SystemC
{

class Gem5Extension: public tlm::tlm_extension<Gem5Extension>
{
  public:
    Gem5Extension(gem5::PacketPtr _packet);

    virtual tlm_extension_base *clone() const;
    virtual void copy_from(const tlm_extension_base &ext);

    static Gem5Extension &getExtension(
            const tlm::tlm_generic_payload *payload);
    static Gem5Extension &getExtension(
            const tlm::tlm_generic_payload &payload);
    gem5::PacketPtr getPacket();

  private:
    gem5::PacketPtr packet;
};

class AtomicExtension: public tlm::tlm_extension<AtomicExtension>
{
  public:
    AtomicExtension(
        std::shared_ptr<gem5::AtomicOpFunctor> amo_op, bool need_return);

    virtual tlm_extension_base *clone() const;
    virtual void copy_from(const tlm_extension_base &ext);

    static AtomicExtension &getExtension(
            const tlm::tlm_generic_payload *payload);
    static AtomicExtension &getExtension(
            const tlm::tlm_generic_payload &payload);

    bool needReturn() const;
    gem5::AtomicOpFunctor* getAtomicOpFunctor() const;

  private:
    std::shared_ptr<gem5::AtomicOpFunctor> _op;
    bool _needReturn;
};

} // namespace Gem5SystemC

#endif // __SYSTEMC_TLM_BRIDGE_SC_EXT_HH__
