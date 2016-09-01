/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "source.h"

Source load_source(const yaml &root)
{
    Source source;
    auto &src = root["source"];
    if (!src.IsDefined())
        return source;

    auto error = "Only one source must be specified";

    Git git;
    EXTRACT_VAR(src, git.url, "git", String);
    EXTRACT_VAR(src, git.branch, "branch", String);
    EXTRACT_VAR(src, git.tag, "tag", String);

    if (git.isValid())
    {
        if (src["file"].IsDefined())
            throw std::runtime_error(error);
        source = git;
    }
    else
    {
        RemoteFile rf;
        EXTRACT_VAR(src, rf.url, "remote", String);

        if (!rf.url.empty())
            source = rf;
        else
            throw std::runtime_error(error);
    }

    return source;
}

void save_source(yaml &root, const Source &source)
{
    auto save_source = overload(
        [&root](const Git &git)
    {
        root["source"]["git"] = git.url;
        if (!git.tag.empty())
            root["source"]["tag"] = git.tag;
        if (!git.branch.empty())
            root["source"]["branch"] = git.branch;
    },
        [&root](const RemoteFile &rf)
    {
        root["source"]["remote"] = rf.url;
    }
    );

    boost::apply_visitor(save_source, source);
}
