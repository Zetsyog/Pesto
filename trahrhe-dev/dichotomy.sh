getbound() {

    typeset Indice="$1"
    typeset dim="$2"
    typeset code="$3"
    typeset NivIf="$4"
    typeset nbre_if
    typeset nbre_for
    typeset cas
    typeset l1
    typeset l2

    if [ "${dim}" = "$((N + 1))" ]; then
        for ((i = 1; i <= $N; i++)); do
            if [ "${argt_if[${i}]}" != "" ]; then
                if_str+="${argt_if[${i}]} && "
            fi
            if_str+="${argt_for[${i}]} && "
        done
        for ((i = 1; i <= $N; i++)); do
            if_str=$(echo "${if_str}" | sed "s~c$((i - 1))~(${argt_str[${i}]})~g")
        done
        if_str=$(echo "${if_str}" | sed "s~ \&\&  \&\& ~ \&\& ~g")
        if_str=$(echo "${if_str}" | sed "s~\(.*\) && )*~\1~")
        BOdyCode+="    if (${if_str})${NEWLINE}"
        if (echo "$parms" | grep -q "NOT_A_PARAMETER"); then
          ranking_call_str+=")"
        else
          ranking_call_str+=", ${parms})"
        fi
        BOdyCode+="    ${ranking_call_str};${NEWLINE}${NEWLINE}"
        if_str=""
        return 0
    fi

    nbre_if=$(echo "$code" | grep -B1 "for (int c$((dim - 1)) " | grep -c " *if ")
    nbre_for=$(echo "$code" | grep -c "for (int c$((dim - 1)) ")
    
    if [ "${NivIf}" = "0" ]; then

        NCas[${Indice}]="${nbre_for}"

        for ((cas = 1; cas <= ${nbre_for}; cas++)); do

            prev_Indice="${Indice}"
            prev_val="${prev_Indice:$((4 * dim - 1)):1}"
            Indice="${prev_Indice:0:$((4 * dim - 1))}"
            Indice="${Indice}$((prev_val + 1))"
            Indice="${Indice}${prev_Indice:$((4 * dim)):2*$((N - dim + 2))}"

            lbound[${CVAR}${Indice}]=$(echo "$code" | grep -m${cas} "for (int c$((dim - 1)) " | tail -1 | sed "s/ *for (int .* = *\(.*\);.*;.*) *{*/\1/")
            ubound[${CVAR}${Indice}]=$(echo "$code" | grep -m${cas} "for (int c$((dim - 1)) " | tail -1 | sed "s/ *for (.*; .* < \(.*\);.*) *{*/\1/" | sed "s/ *for (.*; .* <= \(.*\);.*) *{*/\1+1/")
            
            if [ "${dim}" = "1" ]; then
                ranking_call_str="  rank = ${CVAR}_Ranking(m"
                argt_str[1]="m"
                if [ "${cas}" = "1" ]; then
                    val_init[${cpt_init}]="${lbound[${CVAR}${Indice}]}"
                fi
                if [ "${cas}" = "${nbre_for}" ]; then
                    val_upper[${cpt_init}]="${ubound[${CVAR}${Indice}]}"
                    ((cpt_init++))
                fi
            else
                argt_str[${dim}]="${lbound[${CVAR}${Indice}]}"
                for ((i = 2; i <= ${dim}; i++)); do
                    argt_str[${dim}]=$(echo "${argt_str[${dim}]}" | sed "s~c$((i - 2))~${argt_str[$((i - 1))]}~g")
                done
                ranking_call_str+=", ${argt_str[${dim}]}"
            fi

            condbound[${CVAR}${Indice}]="((c$((dim - 1)) >= ${lbound[${CVAR}${Indice}]}) && (c$((dim - 1)) < ${ubound[${CVAR}${Indice}]}))"

            if [ "${dim}" -gt "1" ]; then
                argt_for[${dim}]+=$(echo "(${condbound[${CVAR}${Indice}]}) && " | sed "s/c0/m/g" | sed "s~c$((dim - 1))~(${argt_str[${dim}]})~g")
            else
                argt_for[${dim}]+=$(echo "(${condbound[${CVAR}${Indice}]}) && " | sed "s/c0/m/g")
            fi

            l1=$(echo "$code" | grep -m${cas} -n "for (int c$((dim - 1)) " | tail -1 | sed "s/\(.*\):.*/\1/")
            ((l1++))

            if [ "${cas}" = "${nbre_for}" ]; then
                l2="\$"
            else
                l2=$(echo "$code" | grep -m$((cas + 1)) -n "for (int c$((dim - 1)) " | tail -1 | sed "s/\(.*\):.*/\1/")
                ((l2--))
            fi

            code_appel=$(echo "$code" | sed -n ${l1},${l2}p)

            getbound "${Indice}" "$((dim + 1))" "$code_appel" "1"

            argt_for[${dim}]=""
        done

    fi

    if [ "${nbre_if}" = "0" ] && [ "${NivIf}" = "1" ]; then

        NCas[${Indice}]="0"

        prev_Indice="${Indice}"
        prev_val="${prev_Indice:$((4 * dim - 3)):1}"
        Indice="${prev_Indice:0:$((4 * dim - 3))}"
        Indice="${Indice}$((prev_val + 1))"
        Indice="${Indice}${prev_Indice:$((4 * dim - 2)):2*$((N - dim + 3))}"
        #    condbound[${CVAR}]=""
        condbound[${CVAR}${Indice}]=""

        getbound "${Indice}" "${dim}" "$code" "0"

    else
        if [ "${NivIf}" = "1" ]; then #nbre_if>0

            NCas[${Indice}]="${nbre_if}"

            for ((cas = 1; cas <= ${nbre_if}; cas++)); do

                prev_Indice="${Indice}"
                prev_val="${prev_Indice:$((4 * dim - 3)):1}"
                Indice="${prev_Indice:0:$((4 * dim - 3))}"
                Indice="${Indice}$((prev_val + 1))"
                Indice="${Indice}${prev_Indice:$((4 * dim - 2)):2*$((N - dim + 3))}"

                condbound[${CVAR}${Indice}]=$(echo "$code" | grep -B1 "for (int c$((dim - 1)) " | grep " *if " | grep -m${cas} " *if " | tail -1 | sed "s/ *if *\(.*\)/\1/" | tr -d "{")
                condbound[${CVAR}${Indice}]=$(echo "${condbound[${CVAR}${Indice}]};" | sed "s~ && ~ and ~g" | ${D2C})

                if [ "${dim}" -gt "1" ]; then
                    argt_if[${dim}]+=$(echo "(${condbound[${CVAR}${Indice}]}) && " | sed "s/c0/m/g" | sed "s~c$((dim - 1))~(${argt_str[${dim}]})~g")
                else
                    argt_if[${dim}]+=$(echo "(${condbound[${CVAR}${Indice}]}) && " | sed "s/c0/m/g")
                    if_init[${cpt_init}]="if (${condbound[${CVAR}${Indice}]})"
                fi

                #l1=$(echo "$code" | grep -B1 -n "for (int c$((dim - 1)) " | grep " if " | grep -m${cas} " if " | tail -1 | sed "s/\(.*\)-.*/\1/")
                l1=$(echo "$code" | grep -B1 -n "for (int c$((dim - 1)) " | grep " *if " | grep -m${cas} " *if " | tail -1 | sed "s/\(^[0-9]*\).*/\1/")
                ((l1++))

                if [ "${cas}" = "${nbre_if}" ]; then
                    l2="\$"
                else
                    #l2=$(echo "$code" | grep -B1 -n "for (int c$((dim - 1)) " | grep " if " | grep -m$((cas + 1)) " if " | tail -1 | sed "s/\(.*\)-.*/\1/")
                    l2=$(echo "$code" | grep -B1 -n "for (int c$((dim - 1)) " | grep " *if " | grep -m$((cas + 1)) " *if " | tail -1 | sed "s/\(^[0-9]*\).*/\1/")
                    ((l2--))
                fi

                code_appel=$(echo "$code" | sed -n ${l1},${l2}p)

                getbound "${Indice}" "${dim}" "$code_appel" "0"

                argt_if[${dim}]=""
            done

        fi

        #argt_if[${dim}]=""
    fi #nbre_if>0
    return 0
}

gencode_dichotomy() {

    typeset -A lbound ubound condbound NCas argt_if argt_for argt_str if_init val_init val_upper 
    NEWLINE=$'\n'
    NivIf=1
    if_str=""
    BOdyCode=""
    cpt_init=1

    for ((i = 1; i <= $N; i++)); do
        argt_if[${i}]=""
        argt_for[${i}]=""
    done

    Indice=$(for ((i = 0; i < $N; i++)); do echo -n "I0F0"; done)

    getbound "${Indice}" "1" "$code" "${NivIf}"

    #echo "********************** Fin recursion *********************"

    echo "long int ${CVAR};"
    echo "long int upper_bound;"
    echo "long int m, rank;"
    echo " "
    
    for ((i = 1; i < ${cpt_init}; i++)); do
        if [ "${if_init[${i}]}" != "" ]; then
            echo "${if_init[${i}]} {"
        fi
        echo "  ${CVAR} = ${val_init[${i}]};"
        echo "  upper_bound = ${val_upper[${i}]};"
        if [ "${if_init[${i}]}" != "" ]; then
            echo "}"
        fi
    done

    echo " "

    echo ""
    echo "  while (${CVAR}+1 < upper_bound) {"
    echo "    m = (${CVAR} + upper_bound)/2;${NEWLINE}"
    echo -n "${BOdyCode}"
    echo "    if (rank <= pc)"
    echo "      ${CVAR} = m;"
    echo "    else"
    echo "      upper_bound = m;"
    echo "  }"
    echo ""
    echo "  return ${CVAR};"
    #echo "} /* end ${CVAR}_trahrhe_$var */"
    echo " "
}
