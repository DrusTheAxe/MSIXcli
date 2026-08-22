// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace MSIX::Deployment
{
inline HRESULT GetResults(
    __FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress* deploymentOperation,
    PCWSTR& errorText,
    wil::unique_hstring& errorTextHString,
    HRESULT& extendedError,
    GUID& activityId)
{
    const HRESULT waitHr{ wil::wait_for_completion_nothrow(deploymentOperation) };
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IDeploymentResult> deploymentResult;
    const HRESULT resultsHr{ deploymentOperation->GetResults(deploymentResult.put()) };
    if (deploymentResult)
    {
        std::ignore = LOG_IF_FAILED(deploymentResult->get_ErrorText(wil::out_param(errorTextHString)));
        errorText = WindowsGetStringRawBuffer(errorTextHString.get(), nullptr);
        std::ignore = LOG_IF_FAILED(deploymentResult->get_ExtendedErrorCode(&extendedError));
        std::ignore = LOG_IF_FAILED(deploymentResult->get_ActivityId(&activityId));
    }
    RETURN_IF_FAILED_MSG(waitHr, "Deployment failed. Extended:0x%08X Text:%ls", extendedError, errorText ? errorText : L"<null>");
    RETURN_IF_FAILED_MSG(resultsHr, "GetResults failed. Extended:0x%08X Text:%ls", extendedError, errorText ? errorText : L"<null>");
    RETURN_IF_FAILED(extendedError);
    return S_OK;
}
}
