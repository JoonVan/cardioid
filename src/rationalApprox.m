function [thisTolerance, hornNumerator, hornDenominator] = rationalApprox(fff, outputName, inputName, lb, ub, numPoints, tolerance, errorType)
  zzz = linspace(-1,1,numPoints)';
  xxx = zzz*(ub-lb)/2 + (ub+lb)/2;
  yyy = fff(xxx);

  for cost=3:64
    [hornNumerator, hornDenominator, thisTolerance] = findBestAtCost(yyy, zzz, lb, ub, cost, errorType);
    numTermCount = rows(hornNumerator);
    denomTermCount = rows(hornDenominator);
    thisTolerance

    if (thisTolerance < tolerance)
      %print it out
      numeratorString = cell(size(hornNumerator));
      denominatorString = cell(size(hornDenominator));
      for ii=1:length(hornNumerator)
        numeratorString{ii} = sprintf("%.17e", hornNumerator(ii));
      endfor
      for ii=1:length(hornDenominator)
        denominatorString{ii} = sprintf("%.17e", hornDenominator(ii));
      endfor
      printf("double %s;\n", outputName);
      printf("{\n");
      printf("   //rationalApprox(%s, \"%s\", \"%s\", %f, %f, %d, %e, \"%s\")\n", func2str(fff), outputName, inputName, lb, ub, numPoints, tolerance, errorType);
      printf("   double __x = %s;\n", inputName);
      printf("   if (useRatpolyApprox && %f <= __x && __x <= %f)\n", lb, ub);
      printf("   {\n");
      printf("      const int pSize = %d;\n", numTermCount);
      printf("      const double pCoeff[] = { %s }\;\n", strjoin(numeratorString, ", "));
      printf("      double numerator=pCoeff[pSize-1];\n");
      printf("      for (int ip=pSize-2; ip>=0; --ip)\n");
      printf("      {\n");
      printf("         numerator = pCoeff[ip] + __x*numerator;\n")
      printf("      }\n");
      if (denomTermCount == 1)
        printf("      %s = numerator;\n", outputName);
      else
        printf("      const int qSize = %d;\n", denomTermCount);
        printf("      double qCoeff[] = { %s };\n", strjoin(denominatorString, ", "));
        printf("      double denominator = qCoeff[qSize-1];\n");
        printf("      for (int iq=qSize-2; iq>=0; --iq)\n");
        printf("      {\n");
        printf("         denominator = qCoeff[iq] + __x*denominator;\n");
        printf("      }\n");
        printf("      %s = numerator/denominator;\n", outputName);
      endif
      printf("   }\n");
      printf("   else\n");
      printf("   {\n\n");
      printf("      %s = ;\n", outputName);
      printf("   }\n");
      printf("}\n");
      break;
    endif
  endfor
endfunction

function [hornNumeratorCoeffs, hornDenominatorCoeffs, bestTolerance] = findBestAtCost(yyy, zzz, lb, ub, maxTermCount, errorType)
  % build the polynomial
  assert(maxTermCount >= 2)
  chebyPoly = zeros(rows(zzz), maxTermCount);
  chebyPoly(:,1) = 1;
  chebyPoly(:,2) = zzz;
  for ii=3:maxTermCount
    chebyPoly(:,ii) = 2.*zzz.*chebyPoly(:,ii-1) - chebyPoly(:,ii-2);
  endfor
  figure(1);
  plot(zzz, chebyPoly);

  funcMin = min(yyy);
  funcMax = max(yyy);
  
  bestChebyCoeffs = zeros(maxTermCount-1);
  bestTolerance = 1e30;
  bestNumTermCount = 0;
  bestDenomTermCount = 0;
  for denomTermCount=1:maxTermCount-1
    numTermCount = maxTermCount - denomTermCount;
    
    [approximation, chebyCoeffs] = buildApproximate(yyy, chebyPoly, numTermCount, denomTermCount);
    rmsErrorScaled = norm(approximation - yyy)/sqrt(rows(yyy))/(funcMax-funcMin);
    maxErrorScaled = max(abs(approximation - yyy))/(funcMax-funcMin);
    if (errorType == "rms")
      errorScaled = rmsErrorScaled;
    else
      errorScaled = maxErrorScaled;
    endif
    if (errorScaled < bestTolerance)
      bestTolerance = errorScaled;
      bestNumTermCount = numTermCount;
      bestDenomTermCount = denomTermCount;
      bestChebyCoeffs = chebyCoeffs;
      figure(3)
      plot(zzz, [yyy approximation]);
      figure(4)
      plot(zzz, approximation-yyy);
    endif

    figure(2);
    plot(zzz, [yyy approximation]);
  endfor

  % build the horn coeff
  zFromCheby = zeros(maxTermCount, maxTermCount);
  zFromCheby(1:2,1:2) = eye(2);
  for ii=3:maxTermCount
    zFromCheby(:,ii) = [0; 2*zFromCheby(1:end-1, ii-1)] - zFromCheby(:, ii-2);
  endfor

  zFromHorn = zeros(maxTermCount, maxTermCount);
  zFromHorn(1,1) = 1;
  for ii=2:maxTermCount
    zFromHorn(:,ii) = [0; zFromHorn(1:end-1, ii-1)]*(ub-lb)/2 + zFromHorn(:,ii-1)*(ub+lb)/2;
  endfor

  % combine them into one nice converter.

  if (diag(diag(zFromHorn) == zFromHorn))
    hornFromCheby = diag(1./diag(zFromHorn)) * zFromCheby;
  else
    oldSetting = warning("query", "singular-matrix-div");
    warning("off", "singular-matrix-div");
    hornFromCheby = zFromHorn \ zFromCheby;
    warning(oldSetting.state, "singular-matrix-div");
  endif
    
  chebyNumeratorCoeffs = bestChebyCoeffs(1:bestNumTermCount);
  chebyDenominatorCoeffs = [ 1; bestChebyCoeffs(bestNumTermCount+1:end) ];
  hornNumeratorCoeffs = hornFromCheby(1:bestNumTermCount,1:bestNumTermCount)*chebyNumeratorCoeffs;
  hornDenominatorCoeffs = hornFromCheby(1:bestDenomTermCount, 1:bestDenomTermCount)*chebyDenominatorCoeffs;
  %zFromCheby
  %zFromHorn
  %hornFromCheby
  %zFromCheby(1:bestNumTermCount,1:bestNumTermCount)
  %chebyNumeratorCoeffs
  %zFromCheby(1:bestNumTermCount,1:bestNumTermCount)*chebyNumeratorCoeffs
  %zFromCheby(1:bestDenomTermCount,1:bestDenomTermCount)
  %chebyDenominatorCoeffs
  %zFromCheby(1:bestDenomTermCount,1:bestDenomTermCount)*chebyDenominatorCoeffs
  %hornNumeratorCoeffs
  %hornDenominatorCoeffs
  hornNumeratorCoeffs = hornNumeratorCoeffs ./ hornDenominatorCoeffs(1);
  hornDenominatorCoeffs = hornDenominatorCoeffs ./ hornDenominatorCoeffs(1);
  hornCoeffs = [hornNumeratorCoeffs; hornDenominatorCoeffs(2:end)];

  
endfunction

function [approximation,coeffs] = buildApproximate(yyy, interpPoly, numTermCount, denomTermCount)

  coeffCount = numTermCount+denomTermCount-1;
  AAA = zeros(rows(yyy), coeffCount);
  bbb = yyy;
  for ii=1:numTermCount
    AAA(:,ii) = interpPoly(:,ii);
  endfor
  for jj=2:denomTermCount
    AAA(:,jj+numTermCount-1) = -yyy.*interpPoly(:,jj);
  endfor

  coeffs = AAA\bbb; % find the coeffs
  approximation = interpPoly(:,1:numTermCount)*coeffs(1:numTermCount) ./ (ones(size(yyy)) + interpPoly(:,2:denomTermCount)*coeffs(numTermCount+1:end));
endfunction
